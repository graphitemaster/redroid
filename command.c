#include "command.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

struct cmd_pool_s {
    pthread_t    handle;
    list_t      *queue;
    cmd_entry_t *current;
    time_t       timeout;
    bool         ready;
};

struct cmd_entry_s {
    void (*entry)(irc_t *, const char *, const char *, const char *);
    irc_t *irc;

    // copied over
    char *channel;
    char *user;
    char *message;
};

cmd_entry_t *cmd_entry_create (
    irc_t      *irc,
    const char *channel,
    const char *user,
    const char *message,
    void      (*func)(irc_t *, const char *, const char *, const char *)
){

    cmd_entry_t *entry = malloc(sizeof(*entry));

    entry->entry   = func;
    entry->irc     = irc;
    entry->channel = strdup(channel);
    entry->user    = strdup(user);
    entry->message = strdup(message);

    return entry;
}

void cmd_entry_destroy(cmd_entry_t *entry) {
    if (!entry)
        return;

    free(entry->channel);
    free(entry->user);
    free(entry->message);
    free(entry);
}

cmd_pool_t *cmd_pool_create(void) {
    cmd_pool_t *pool = malloc(sizeof(*pool));
    pool->queue   = list_create();
    pool->ready   = false;
    pool->current = NULL;

    return pool;
}

void cmd_pool_destroy(cmd_pool_t *pool) {
    // wait for the thread to complete
    pool->ready = false;
    pthread_join(pool->handle, NULL);

    // collect unprocessed ones
    list_iterator_t *it = list_iterator_create(pool->queue);
    while (!list_iterator_end(it))
        cmd_entry_destroy(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(pool->queue);
    free(pool);
}

void cmd_pool_queue(cmd_pool_t *pool, cmd_entry_t *entry) {
    list_push(pool->queue, entry);
}

static void *cmd_pool_dispatcher(void *data) {
    cmd_pool_t *pool  = data;
    list_t     *queue = pool->queue;

    while (pool->ready) {
        if (!queue)
            continue;

        cmd_entry_t *entry = list_pop(queue);

        if (entry) {
            pool->timeout = time(0);
            pool->current = entry;

            entry->entry(entry->irc, entry->channel, entry->user, entry->message);

            cmd_entry_destroy(entry);
        }
    }

    return NULL;
}

static void cmd_pool_signalhandler(int sig) {
    pthread_exit(NULL);
}

void cmd_pool_begin(cmd_pool_t *pool) {
    signal(SIGUSR2, &cmd_pool_signalhandler);
    pthread_create(&pool->handle, NULL, &cmd_pool_dispatcher, pool);
    pool->ready = true;
    printf("    queue   => running\n");
}

void cmd_pool_process(cmd_pool_t *pool) {
    if (!pool || !pool->current)
        return;

    if (difftime(time(0), pool->timeout) >= 5) {
        irc_write (
            pool->current->irc,
            pool->current->channel,
            "%s: command timedout",
            pool->current->user
        );

        // restart it
        pthread_kill(pool->handle, SIGUSR2);
        cmd_pool_begin(pool);
        pool->current = NULL;
    }
}

bool cmd_pool_ready(cmd_pool_t *pool) {
    return pool ? pool->ready : false;
}
