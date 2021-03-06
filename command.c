#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pthread.h>

#include "moduleman.h"
#include "command.h"

#define COMMAND_TIMEOUT_SECONDS 5

static bool cmd_channel_crashed = false;

struct cmd_link_s {
    cmd_entry_t *data;
    cmd_link_t  *next;
};

struct cmd_channel_s {
    pthread_t       thread;
    pthread_mutex_t mutex;
    pthread_cond_t  waiter;
    timer_t         timerid;
    cmd_link_t     *head;
    cmd_link_t     *tail;
    cmd_link_t     *volatile rdintent;
    volatile bool   rdend;
    volatile bool   wrend;
    void          (*destroy)(cmd_entry_t *);
    bool            ready;
    cmd_entry_t    *cmd_entry;
    pthread_mutex_t cmd_mutex;
};

struct cmd_entry_s {
    cmd_channel_t *associated;
    module_t      *instance;
    string_t      *channel;
    string_t      *user;
    string_t      *message;
};

cmd_link_t *cmd_link_create(void) {
    return calloc(1, sizeof(cmd_link_t));
}

void cmd_link_destroy(cmd_link_t *link, void (*destroy)(cmd_entry_t *)) {
    if (destroy)
        destroy(link->data);
    free(link);
}

bool cmd_channel_exclusive(cmd_channel_t *channel) {
    return channel->rdintent != NULL;
}

cmd_channel_t *cmd_channel_create(void) {
    cmd_channel_t *channel = malloc(sizeof(*channel));

    pthread_mutex_init(&channel->mutex,     NULL);
    pthread_mutex_init(&channel->cmd_mutex, NULL);
    pthread_cond_init (&channel->waiter,    NULL);

    channel->head      = cmd_link_create();
    channel->tail      = channel->head;
    channel->rdintent  = NULL;
    channel->rdend     = false;
    channel->wrend     = false;
    channel->destroy   = NULL;
    channel->ready     = false;
    channel->cmd_entry = NULL;

    return channel;
}

void cmd_channel_destroy(cmd_channel_t *channel) {
    cmd_channel_wrclose(channel);

    if (channel->ready) {
        pthread_join(channel->thread, NULL);
        timer_delete(channel->timerid);
    }

    pthread_mutex_destroy(&channel->mutex);
    pthread_mutex_destroy(&channel->cmd_mutex);
    pthread_cond_destroy (&channel->waiter);

    cmd_link_t *link = channel->head;
    while (link) {
        cmd_link_t *next = link->next;
        if (next && channel->destroy)
            channel->destroy(link->data);
        cmd_link_destroy(link, &cmd_entry_destroy);
        link = next;
    }

    free(channel);
}

bool cmd_channel_push(cmd_channel_t *channel, cmd_entry_t *entry) {
    cmd_link_t *tail = channel->tail;
    tail->data = entry;
    channel->tail = channel->tail->next = cmd_link_create();

    if (channel->rdintent == tail) {
        pthread_mutex_lock(&channel->mutex);
        if (channel->rdintent == tail)
            pthread_cond_signal(&channel->waiter);
        pthread_mutex_unlock(&channel->mutex);
    }
    return !channel->rdend;
}

bool cmd_channel_pop(cmd_channel_t *channel, cmd_entry_t **output) {
    channel->rdintent = channel->head;
    if (channel->head == channel->tail) {
        if (channel->wrend)
            return false;

        pthread_mutex_lock(&channel->mutex);
        if (channel->wrend) {
            pthread_mutex_unlock(&channel->mutex);
            return false;
        }

        while (!channel->wrend && channel->head == channel->tail)
            pthread_cond_wait(&channel->waiter, &channel->mutex);

        channel->rdintent = NULL;
        pthread_mutex_unlock(&channel->mutex);
        if (channel->wrend)
            return false;
    }

    channel->rdintent = NULL;
    cmd_link_t *head = channel->head;
    channel->head = channel->head->next;
    *output = head->data;
    cmd_link_destroy(head, channel->destroy);

    return true;
}

void cmd_channel_wrclose(cmd_channel_t *channel) {
    channel->wrend = true;
    if (channel->rdintent == channel->tail) {
        pthread_mutex_lock(&channel->mutex);
        if (channel->rdintent == channel->tail)
            pthread_cond_signal(&channel->waiter);
        pthread_mutex_unlock(&channel->mutex);
    }
}

void cmd_channel_rdclose(cmd_channel_t *channel) {
    channel->rdend = true;
}

#define strchk(X) ((X) && strlen(X))

cmd_entry_t *cmd_entry_create(
    cmd_channel_t *associated,
    module_t      *module,
    const char    *channel,
    const char    *user,
    const char    *message
) {
    cmd_entry_t *entry = malloc(sizeof(*entry));

    entry->associated = associated;
    entry->instance   = module;
    entry->channel    = string_create(channel);
    /* Users and messages can be empty */
    entry->user       = strchk(user) ? string_create(user) : NULL;
    entry->message    = strchk(message) ? string_create(message) : NULL;;

    return entry;
}

void cmd_entry_destroy(cmd_entry_t *entry) {
    if (!entry)
        return;

    if (entry->channel) string_destroy(entry->channel);
    if (entry->user)    string_destroy(entry->user);
    if (entry->message) string_destroy(entry->message);

    module_t *load = entry->instance;
    module_mem_destroy(load);

    free(entry);
}

static void cmd_channel_signalhandle_quit(int sig) {
    /* not a timeout, i.e module crashed */
    if (sig != SIGUSR2)
        cmd_channel_crashed = true;
    pthread_exit(NULL);
}

static void cmd_channel_signalhandle_timeout(int sig, siginfo_t *si, void *ignore) {
    (void)sig; /* ignored */
    (void)ignore; /* ignored */

    cmd_channel_t *channel = si->si_value.sival_ptr;

    /* Did a command timeout? */
    pthread_mutex_lock(&channel->cmd_mutex);
    module_t *instance = (channel->cmd_entry) ? channel->cmd_entry->instance : NULL;
    if (!instance) {
        pthread_mutex_unlock(&channel->cmd_mutex);
        return;
    }

    pthread_kill(channel->thread, SIGUSR2);
    pthread_join(channel->thread, NULL);
    pthread_mutex_unlock(&channel->cmd_mutex);

    cmd_entry_t *entry = channel->cmd_entry;
    channel->cmd_entry = NULL;

    /*
     * Don't write timeout messages for modules which are interval based.
     * For example the SVN update module may timeout if the server goes
     * down and we don't want those showing up here.
     */
    if (!entry->instance->interval) {
        irc_t      *irc     = entry->instance->instance;
        const char *channel = string_contents(entry->channel);
        const char *user    = string_contents(entry->user);

        irc_write(irc, channel, "%s: command %s", user,
            cmd_channel_crashed ? "crashed" : "timeout");
    }

    cmd_entry_destroy(entry);

    channel->rdend = false;
    cmd_channel_begin(channel);
}

static void *cmd_channel_threader(void *data) {
    cmd_channel_t *channel = data;
    cmd_entry_t   *entry   = NULL;

    while (cmd_channel_pop(channel, &entry)) {
        module_t *module = entry->instance;

        /*
         * unloaded module references cannot be accessed. We'll just
         * ignore any commands that still reference old modules.
         */
        if (module_manager_unloaded_find(module->instance->moduleman, module))
            continue;

        if (module->enter) {
            module_singleton_set(module);
            channel->cmd_entry = entry;
            module->memory     = list_create();

            struct itimerspec its = {
                .it_value = {
                    .tv_sec  = COMMAND_TIMEOUT_SECONDS
                },
                .it_interval = {
                    .tv_sec  = COMMAND_TIMEOUT_SECONDS,
                    .tv_nsec = 0
                }
            };

            if (timer_settime(channel->timerid, 0, &its, NULL) == -1)
                abort();

            module->enter(
                module->instance,
                string_contents(entry->channel),
                string_contents(entry->user),
                string_contents(entry->message)
            );

            pthread_mutex_lock(&channel->cmd_mutex);
            cmd_entry_destroy(entry);

            /* Disarm timer */
            its.it_value.tv_sec = 0;
            timer_settime(channel->timerid, 0, &its, NULL);

            channel->cmd_entry = NULL;
            pthread_mutex_unlock(&channel->cmd_mutex);
        }

        irc_unqueue(module->instance);
    }

    if (entry)
        module_manager_unloaded_clear(entry->instance->instance->moduleman);
    cmd_channel_rdclose(channel);
    return NULL;
}

static bool cmd_channel_init(cmd_channel_t *channel) {
    /* Modules can segfault. We handle for that here */
    signal(SIGSEGV, &cmd_channel_signalhandle_quit);
    signal(SIGUSR2, &cmd_channel_signalhandle_quit);

    /* Establish a signal handler for module timeouts */
    struct sigaction sa = {
        .sa_flags     = SA_SIGINFO,
        .sa_sigaction = &cmd_channel_signalhandle_timeout,
    };

    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        return false;

    /* Construct the timeout timer */
    struct sigevent e = {
        .sigev_notify          = SIGEV_SIGNAL,
        .sigev_signo           = SIGALRM,
        .sigev_value.sival_ptr = channel
    };

    if (timer_create(CLOCK_REALTIME, &e, &channel->timerid) == -1)
        return false;

    return true;
}

bool cmd_channel_begin(cmd_channel_t *channel) {
    if (!channel->ready && !cmd_channel_init(channel))
        return false;

    /* coming back from a crash or timeout? */
    if (channel->ready) {
        /* reestablish signal handler because SIG_DFL */
        signal(SIGUSR2, &cmd_channel_signalhandle_quit);
        signal(SIGSEGV, &cmd_channel_signalhandle_quit);
        cmd_channel_crashed = false;
    }

    if (pthread_create(&channel->thread, NULL, &cmd_channel_threader, channel) == 0) {
        printf("    queue    => %s\n", (channel->ready) ? "restarted" : "running");
        return channel->ready = true;
    }

    return false;
}

bool cmd_channel_ready(cmd_channel_t *channel) {
    return channel->ready;
}
