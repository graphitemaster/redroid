#include "command.h"
#include "string.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

struct cmd_link_s {
    cmd_entry_t *data;
    cmd_link_t  *next;
};

struct cmd_channel_s {
    pthread_t       thread;
    pthread_mutex_t mutex;
    pthread_cond_t  waiter;
    cmd_link_t     *head;
    cmd_link_t     *tail;
    cmd_link_t     *volatile rdintent;
    volatile bool   rdend;
    volatile bool   wrend;
    void          (*destroy)(cmd_entry_t *);
    bool            ready;
    time_t          cmd_time;
    cmd_entry_t    *cmd_entry;
    pthread_mutex_t cmd_mutex;
};

struct cmd_entry_s {
    cmd_channel_t *associated; // to help with some stuff
    module_t      *instance;
    string_t      *channel;
    string_t      *user;
    string_t      *message;
};

cmd_link_t *cmd_link_create(void) {
    return memset(malloc(sizeof(cmd_link_t)), 0, sizeof(cmd_link_t));
}

void cmd_link_destroy(cmd_link_t *link, void (*destroy)(cmd_entry_t *)) {
    if (destroy)
        destroy(link->data);
    free(link);
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
    channel->cmd_time  = 0;
    channel->cmd_entry = NULL;

    return channel;
}

void cmd_channel_destroy(cmd_channel_t *channel) {
    cmd_channel_wrclose(channel);
    if (cmd_channel_timeout(channel))
        cmd_entry_destroy(channel->cmd_entry);
    else if (channel->ready)
        pthread_join(channel->thread, NULL);

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

        if (channel->head == channel->tail)
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
    entry->user       = string_create(user);

    // messages can be empty
    if (message && strlen(message))
        entry->message = string_create(message);
    else
        entry->message = NULL;

    return entry;
}

void cmd_entry_destroy(cmd_entry_t *entry) {
    if (!entry)
        return;

    if (entry->channel) string_destroy(entry->channel);
    if (entry->user)    string_destroy(entry->user);
    if (entry->message) string_destroy(entry->message);

    module_mem_destroy(entry->instance);

    free(entry);
}

static void cmd_channel_signalhandle(int sig) {
    pthread_exit(NULL);
}

static void *cmd_channel_threader(void *data) {
    cmd_channel_t *channel = data;
    cmd_entry_t   *entry   = NULL;

    while (cmd_channel_pop(channel, &entry)) {
        module_t *module = entry->instance;

        //
        // unloaded module references cannot be accessed. We'll just
        // ignore any commands that still reference old modules.
        //
        if (module_unloaded(module->instance, module))
            continue;

        if (module && module->enter) {
            channel->cmd_time  = time(NULL);
            channel->cmd_entry = entry;
            *module_get()      = module; // save current module instance
            module->memory     = module_mem_create(module);
            module->enter(
                module->instance,
                string_contents(entry->channel),
                string_contents(entry->user),
                string_contents(entry->message)
            );

            pthread_mutex_lock(&channel->cmd_mutex);
            cmd_entry_destroy(entry);
            channel->cmd_time  = 0;
            channel->cmd_entry = NULL;
            pthread_mutex_unlock(&channel->cmd_mutex);
        }
    }

    cmd_channel_rdclose(channel);
    return NULL;
}

bool cmd_channel_begin(cmd_channel_t *channel) {
    //
    // if a module segfaults or times out the handler will gracefully
    // (while cleaning up resources) get rid of it. Without bringing
    // the whole bot process down.
    //
    signal(SIGUSR2, &cmd_channel_signalhandle);
    signal(SIGSEGV, &cmd_channel_signalhandle);

    if (pthread_create(&channel->thread, NULL, &cmd_channel_threader, channel) == 0) {
        printf("    queue    => %s\n", (channel->ready) ? "restarted" : "running");
        return channel->ready = true;
    }

    return false;
}

bool cmd_channel_ready(cmd_channel_t *channel) {
    return channel->ready;
}

bool cmd_channel_timeout(cmd_channel_t *channel) {
    module_t *instance;
    if (!channel->cmd_time || channel->cmd_time + 3 >= time(NULL))
        return false;

    // a command timed out:
    pthread_mutex_lock(&channel->cmd_mutex);
    instance = channel->cmd_entry->instance;
    if (!instance) {
        pthread_mutex_unlock(&channel->cmd_mutex);
        return false;
    }

    module_mem_mutex_lock(instance);
    // it's possible the thread locked the mutex first, which means the command
    // took _exactly_ as much time as allowed, so we need to recheck
    // for whether the command actually did time out:
    if (!channel->cmd_time || channel->cmd_time + 3 >= time(NULL)) {
        // it didn't
        pthread_mutex_unlock(&channel->cmd_mutex);
        module_mem_mutex_unlock(instance);
        return false;
    }
    // now we send the kill signal

    pthread_kill(channel->thread, SIGUSR2);
    pthread_join(channel->thread, NULL);
    pthread_mutex_unlock(&channel->cmd_mutex);
    module_mem_mutex_unlock(instance);
    return true;
}

void cmd_channel_process(cmd_channel_t *channel) {
    if (!cmd_channel_timeout(channel))
        return;

    // the entry is ours now, we need to clean this up:
    cmd_entry_t *entry = channel->cmd_entry;
    channel->cmd_entry = NULL;
    channel->cmd_time  = 0;

    irc_write(
        entry->instance->instance,
        string_contents(entry->channel),
        "%s: command timed out",
        string_contents(entry->user)
    );

    cmd_entry_destroy(entry);

    // reopen the reading end
    channel->rdend = false;
    cmd_channel_begin(channel);
}
