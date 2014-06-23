#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* pipe, close, write, read */
#include <fcntl.h>  /* fcntl, F_GETGL, F_SETFL */
#include <poll.h>   /* poll, POLLIN, POLLOUT */

#include "list.h"
#include "ircman.h"
#include "command.h"
#include "access.h"

typedef struct {
    irc_t **data;
    size_t  size;
    size_t  reserved;
} irc_instances_t;

struct irc_manager_s {
    irc_instances_t *instances;
    cmd_channel_t   *commander;
    struct pollfd   *polls;
    int              wakefds[2];
};

static irc_instances_t *irc_instances_create(void) {
    irc_instances_t *instances = malloc(sizeof(*instances));

    instances->size     = 0;
    instances->reserved = 1;
    instances->data     = malloc(sizeof(irc_t*));

    return instances;
}

static list_t *irc_instances_destroy(irc_instances_t *instances, bool restart) {
    list_t *list = (restart) ? list_create() : NULL;

    for (size_t i = 0; i < instances->size; i++) {
        if (!instances->data[i]->ready) {
            irc_destroy(instances->data[i], NULL, NULL);
            continue;
        }

        if (restart) {
            irc_manager_restart_t *restdata = malloc(sizeof(*restdata));
            char                  *restname = NULL;
            sock_restart_t         restinfo;

            irc_destroy(instances->data[i], &restinfo, &restname);

            restdata->fd   = restinfo.fd;
            restdata->name = restname;
            restdata->ssl  = restinfo.ssl;
            restdata->size = restinfo.size;
            restdata->data = restinfo.data;
            list_push(list, restdata);
        } else {
            irc_destroy(instances->data[i], NULL, NULL);
        }
    }

    free(instances->data);
    free(instances);
    return list;
}

static void irc_instances_push(irc_instances_t *instances, irc_t *instance) {
    if (instances->size + 1 >= instances->reserved) {
        instances->reserved <<= 1;
        instances->data       = realloc(instances->data, instances->reserved * sizeof(irc_t *));
    }

    instances->data[instances->size++] = instance;
}

static irc_t *irc_instances_find(irc_instances_t *instances, const char *name) {
    for (size_t i = 0; i < instances->size; i++) {
        if (instances->data[i] && !strcmp(instances->data[i]->name, name))
            return instances->data[i];
    }
    return NULL;
}

static bool irc_manager_stage(irc_manager_t *manager) {
    if (!(manager->polls = malloc(sizeof(struct pollfd) * (manager->instances->size + 1))))
        return false;

    if (pipe(manager->wakefds) == -1) {
        free(manager->polls);
        return false;
    }

    /* Only the read end */
    manager->polls[0].fd     = manager->wakefds[0];
    manager->polls[0].events = POLLIN | POLLPRI;

    /*
     * The read and write ends of the pipe need to be non blocking otherwise
     * the signal handler could deadlock if we get N signals (where N is
     * the size of the pipe buffer) before poll gets called.
     */
    int flags = fcntl(manager->wakefds[0], F_GETFL);
    if (flags == -1)
        goto self_pipe_error;
    flags |= O_NONBLOCK;
    if (fcntl(manager->wakefds[0], F_SETFL, flags) == -1)
        goto self_pipe_error;

    flags = fcntl(manager->wakefds[1], F_GETFL);
    if (flags == -1)
        goto self_pipe_error;
    flags |= O_NONBLOCK;
    if (fcntl(manager->wakefds[1], F_SETFL, flags) == -1)
        goto self_pipe_error;

    for (size_t i = 0; i < manager->instances->size; i++) {
        manager->polls[1+i].fd     = sock_getfd(manager->instances->data[i]->sock);
        manager->polls[1+i].events = POLLIN | POLLPRI;
    }

    /* Begin the command message channel if it isn't already ready */
    if (!cmd_channel_ready(manager->commander))
        cmd_channel_begin(manager->commander);

    return true;

self_pipe_error:
    free(manager->polls);
    close(manager->wakefds[0]);
    close(manager->wakefds[1]);
    free(manager->polls);

    return false;
}

static void irc_manager_cleanup(irc_manager_t *manager) {
    cmd_channel_destroy(manager->commander);
    irc_instances_destroy(manager->instances, false);

    free(manager->polls);
    free(manager);
}

irc_manager_t *irc_manager_create(void) {
    irc_manager_t *man = malloc(sizeof(*man));
    if (!man)
        return NULL;

    man->instances  = irc_instances_create();
    man->commander  = cmd_channel_create();
    man->polls      = NULL;
    man->wakefds[0] = -1;
    man->wakefds[1] = -1;

    return man;
}

typedef struct {
    irc_t    *instance;
    string_t *message;
} irc_manager_broadcast_t;

void irc_manager_broadcast(irc_manager_t *manager, const char *message, ...) {
    string_t *string = string_construct();
    va_list   va;

    va_start(va, message);
    string_vcatf(string, message, va);

    for (size_t i = 0; i < manager->instances->size; i++) {
        irc_t *irc = manager->instances->data[i];
        hashtable_foreach(irc->channels,
            &((irc_manager_broadcast_t) {
                .instance = irc,
                .message  = string
            }),
            lambda void(irc_channel_t *channel, irc_manager_broadcast_t *caster)
                => irc_write(caster->instance, channel->channel, string_contents(caster->message));
        );
    }

    va_end(va);
    string_destroy(string);
}

void irc_manager_wake(irc_manager_t *manager) {
    write(manager->wakefds[1], "wakeup", 6);
}

void irc_manager_destroy(irc_manager_t *manager) {
    irc_manager_wake(manager);
    irc_manager_cleanup(manager);
}

list_t *irc_manager_restart(irc_manager_t *manager) {
    irc_manager_wake(manager);
    cmd_channel_destroy(manager->commander);
    list_t *list = irc_instances_destroy(manager->instances, true);

    if (manager->polls)
        free(manager->polls);

    free(manager);
    return list;
}

typedef struct {
    irc_manager_t *manager;
    module_t      *module;
    irc_t         *instance;
    cmd_entry_t *(*make)(cmd_channel_t *, const char *, module_t *, irc_message_t *);
} irc_manager_foreach_t;

void irc_manager_process(irc_manager_t *manager) {
    if (!cmd_channel_ready(manager->commander)) {
        if (!irc_manager_stage(manager))
            abort();
        return;
    }

    /*
     * Unqueue anything left and calculate the shortest poll timeout for interval
     * modules.
     */
    unsigned int ts = ~0u;
    for (size_t i = 0; i < manager->instances->size; i++) {
        irc_t *instance = manager->instances->data[i];
        irc_unqueue(instance);
        unsigned int timeout = module_manager_timeout(instance->moduleman);
        if (timeout < ts)
            ts = timeout;
    }

    int wait = poll(manager->polls, manager->instances->size + 1, ts != ~0u ? (int)(ts * 1000) : -1);
    if (wait == -1)
        return;

    /*
     * If we don't drain the pipe then we'll continue to have an event
     * on it
     */
    if (manager->polls[0].revents & POLLIN) {
        char buffer[6];
        read(manager->wakefds[0], buffer, sizeof(buffer));
    }

    for (size_t i = 0; i < manager->instances->size; i++) {
        irc_t *instance = manager->instances->data[i];
        if (manager->polls[1+i].revents & (POLLIN | POLLOUT))
            irc_process(instance, manager->commander);

        if (!instance->syncronized)
            continue;

        list_foreach(instance->moduleman->modules,
            &((irc_manager_foreach_t) {
                .manager  = manager,
                .instance = instance
            }),
            lambda void(module_t *module, irc_manager_foreach_t *foreach) {
                if (*module->match != '\0')
                    return;

                cmd_entry_t *(*make)(cmd_channel_t *, const char *, module_t *, irc_message_t *) =
                    lambda cmd_entry_t *(cmd_channel_t *channel, const char *chan, module_t *module, irc_message_t *message)
                        => return cmd_entry_create(channel, module, chan, message->nick, message->content);;

                foreach->module = module;
                foreach->make   = make;

                /* We must broadcast these on all channels */
                hashtable_foreach(foreach->instance->channels, foreach,
                    lambda void(irc_channel_t *channel, irc_manager_foreach_t *foreach) {
                        if (access_ignore(channel->instance, channel->message.nick))
                            return;
                        if (foreach->module->interval == 0) {
                            cmd_channel_push(foreach->manager->commander,
                                foreach->make(foreach->manager->commander, channel->channel, foreach->module, &channel->message));
                            /* Always modules need clearing, interval ones don't */
                            irc_message_clear(&channel->message);
                        } else if (difftime(time(0), foreach->module->lastinterval) >= foreach->module->interval) {
                            foreach->module->lastinterval = time(0);
                            cmd_channel_push(foreach->manager->commander,
                                foreach->make(foreach->manager->commander, channel->channel, foreach->module, &channel->message));
                        }
                    }
                );
            }
        );
    }
}

irc_t *irc_manager_find(irc_manager_t *manager, const char *name) {
    return irc_instances_find(manager->instances, name);
}

void irc_manager_add(irc_manager_t *manager, irc_t *instance) {
    instance->manager = manager;
    irc_instances_push(manager->instances, instance);
}

bool irc_manager_empty(irc_manager_t *manager) {
    return manager->instances->size == 0;
}
