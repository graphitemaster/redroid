#include "list.h"
#include "ircman.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <poll.h>

typedef struct {
    irc_t **data;
    size_t  size;
    size_t  reserved;
} irc_instances_t;

struct irc_manager_s {
    irc_instances_t *instances;
    cmd_channel_t   *commander;
    struct pollfd   *polls;
};

static irc_instances_t *irc_instances_create(void) {
    irc_instances_t *instances = malloc(sizeof(*instances));

    instances->size     = 0;
    instances->reserved = 1;
    instances->data     = malloc(sizeof(irc_t*));

    return instances;
}

static void irc_instances_destroy(irc_instances_t *instances, bool restart) {
    for (size_t i = 0; i < instances->size; i++)
        irc_destroy(instances->data[i], restart);
    free(instances->data);
    free(instances);
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
        if (!strcmp(instances->data[i]->name, name))
            return instances->data[i];
    }
    return NULL;
}

static void irc_manager_stage(irc_manager_t *manager) {
    manager->polls = malloc(sizeof(struct pollfd) * manager->instances->size);

    for (size_t i = 0; i < manager->instances->size; i++) {
        manager->polls[i].fd     = sock_getfd(manager->instances->data[i]->sock);
        manager->polls[i].events = POLLIN | POLLPRI;
    }

    // begin the channel
    if (!cmd_channel_ready(manager->commander))
        cmd_channel_begin(manager->commander);
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

void irc_manager_destroy(irc_manager_t *manager) {
    irc_instances_destroy(manager->instances, false);
    cmd_channel_destroy(manager->commander);

    if (manager->polls)
        free(manager->polls);

    free(manager);
}

list_t *irc_manager_restart(irc_manager_t *manager) {
    list_t *list = list_create();
    for (size_t i = 0; i < manager->instances->size; i++) {
        irc_manager_restart_t *r = malloc(sizeof(*r));
        r->fd   = sock_getfd(manager->instances->data[i]->sock);
        r->name = strdup(manager->instances->data[i]->name);
        list_push(list, r);
    }

    irc_instances_destroy(manager->instances, true);
    cmd_channel_destroy(manager->commander);

    if (manager->polls)
        free(manager->polls);

    free(manager);

    return list;
}

void irc_manager_process(irc_manager_t *manager) {
    if (!cmd_channel_ready(manager->commander))
        irc_manager_stage(manager);

    int wait = poll(manager->polls, manager->instances->size, -1);
    if (wait == 0 || wait == -1)
        return;

    for (size_t i = 0; i < manager->instances->size; i++) {
        if (manager->polls[i].revents & POLLIN ||
            manager->polls[i].revents & POLLOUT)
            irc_process(manager->instances->data[i], manager->commander);
    }
}

irc_t *irc_manager_find(irc_manager_t *manager, const char *name) {
    return irc_instances_find(manager->instances, name);
}

void irc_manager_add(irc_manager_t *manager, irc_t *instance) {
    irc_instances_push(manager->instances, instance);
}

bool irc_manager_empty(irc_manager_t *manager) {
    return manager->instances->size == 0;
}
