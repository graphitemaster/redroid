#include "command.h"
#include "string.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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
};

struct cmd_entry_s {
    cmd_channel_t *associated; // to help with some stuff
    irc_t         *instance;   // which IRC instance this is associated with
    void         (*method)(irc_t *, const char *, const char *, const char *);
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

    pthread_mutex_init(&channel->mutex, NULL);
    pthread_cond_init (&channel->waiter, NULL);

    channel->head     = cmd_link_create();
    channel->tail     = channel->head;
    channel->rdintent = NULL;
    channel->rdend    = false;
    channel->wrend    = false;
    channel->destroy  = NULL;
    channel->ready    = false;

    return channel;
}

void cmd_channel_destroy(cmd_channel_t *channel) {
    pthread_mutex_destroy(&channel->mutex);
    pthread_cond_destroy (&channel->waiter);

    cmd_link_t *link = channel->head;
    while (link) {
        cmd_link_t *next = link->next;
        if (next && channel->destroy)
            channel->destroy(link->data);
        cmd_link_destroy(link, NULL);
        link = next;
    }
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
            pthread_cond_wait(&channel->waiter, &channel->mutex);
        pthread_mutex_unlock(&channel->mutex);
    }
}

void cmd_channel_rdclose(cmd_channel_t *channel) {
    channel->rdend = true;
}

cmd_entry_t *cmd_entry_create(
    cmd_channel_t *associated,
    irc_t         *irc,
    const char    *channel,
    const char    *user,
    const char    *message,
    void         (*method)(irc_t *, const char *, const char *, const char *)
) {
    cmd_entry_t *entry = malloc(sizeof(*entry));

    entry->associated = associated;
    entry->instance   = irc;
    entry->method     = method;
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
    if (entry->channel) string_destroy(entry->channel);
    if (entry->user)    string_destroy(entry->user);
    if (entry->message) string_destroy(entry->message);

    free(entry);
}

static void cmd_channel_signalhandle(int sig) {
    (void)sig;
    pthread_exit(NULL);
}

static void *cmd_channel_threader(void *data) {
    cmd_channel_t *channel = data;
    cmd_entry_t   *entry   = NULL;

    for(;;) {
        while (cmd_channel_pop(channel, &entry)) {
            if (entry->method) {
                entry->method(
                    entry->instance,
                    string_contents(entry->channel),
                    string_contents(entry->user),
                    string_contents(entry->message)
                );
                cmd_entry_destroy(entry);
            }
        }
    }

    return NULL;
}

bool cmd_channel_begin(cmd_channel_t *channel) {
    signal(SIGUSR2, &cmd_channel_signalhandle);
    pthread_create(&channel->thread, NULL, &cmd_channel_threader, channel);
    printf("    queue   => running\n");
    return channel->ready = true;
}

bool cmd_channel_ready(cmd_channel_t *channel) {
    return channel->ready;
}

void cmd_channel_process(cmd_channel_t *channel) {
    // TODO: consult blub
}
