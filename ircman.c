#include "list.h"
#include "ircman.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

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
    list_t *list = NULL;
    if (restart)
        list = list_create();
    for (size_t i = 0; i < instances->size; i++) {
        if (restart && instances->data[i]->ready) {
            irc_manager_restart_t *r = malloc(sizeof(*r));
            r->fd   = sock_getfd(instances->data[i]->sock);
            r->name = strdup(instances->data[i]->name);
            list_push(list, r);
        }
        irc_destroy(instances->data[i], restart);
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
    manager->polls = malloc(sizeof(struct pollfd) * (manager->instances->size + 1));

    /*
     * Self-pipe to do awake in signal handler for poll. This works because
     * UNIX is awesome.
     */
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

    // begin the channel
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

irc_manager_t *irc_manager_create(void) {
    irc_manager_t *man = malloc(sizeof(*man));
    if (!man)
        return NULL;

    man->instances = irc_instances_create();
    man->commander = cmd_channel_create();
    man->polls     = NULL;

    return man;
}

void irc_manager_wake(irc_manager_t *manager) {
    if (write(manager->wakefds[1], "wakeup", 6) == -1) {
        /* Something went terribly wrong */
        raise(SIGUSR1);
    }
}

void irc_manager_destroy(irc_manager_t *manager) {
    irc_manager_wake(manager);
    cmd_channel_destroy(manager->commander);
    irc_instances_destroy(manager->instances, false);

    if (manager->polls)
        free(manager->polls);

    free(manager);
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

void irc_manager_process(irc_manager_t *manager) {
    if (!cmd_channel_ready(manager->commander)) {
        if (!irc_manager_stage(manager))
            abort();
        return;
    }

    /*
     * Process any data that may exist from a restart for instance or
     * if a module timed out.
     */
    size_t total = manager->instances->size;
    for (size_t i = 0; i < total; i++)
        while (irc_queue_dequeue(manager->instances->data[i]))
            ;

    int wait = poll(manager->polls, manager->instances->size + 1, -1);
    if (wait == 0 || wait == -1)
        return;

    for (size_t i = 0; i < manager->instances->size; i++) {
        if (manager->polls[1+i].revents & POLLIN ||
            manager->polls[1+i].revents & POLLOUT)
            irc_process(manager->instances->data[i], manager->commander);
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
