#include "list.h"
#include "ircman.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>

#ifdef REDROID_USE_EPOLL
#   include <sys/epoll.h>
#   include <unistd.h>
#endif

struct irc_manager_s {
    list_t          *instances;
    list_iterator_t *iterator; // cached iterator
    cmd_channel_t   *commander;
    int              epollfd;
};

irc_manager_t *irc_manager_create(void) {
    irc_manager_t *man = malloc(sizeof(*man));
    if (!man)
        return NULL;

    if (!(man->instances = list_create())) {
        free(man);
        return NULL;
    }

    man->iterator  = NULL;
    man->commander = cmd_channel_create();

    return man;
}

void irc_manager_destroy(irc_manager_t *manager) {
    list_iterator_t *it = manager->iterator;
    if (it) {
        list_iterator_reset(it);
        while (!list_iterator_end(it))
            irc_destroy(list_iterator_next(it));
        list_iterator_destroy(it);
    }

    cmd_channel_destroy(manager->commander);
    list_destroy(manager->instances);
    free(manager);
#ifdef REDROID_USE_EPOLL
    close(manager->epollfd);
#endif
}

irc_t *irc_manager_find(irc_manager_t *manager, const char *name) {
    list_iterator_t *it = manager->iterator;
    list_iterator_reset(it);
    while (!list_iterator_end(it)) {
        irc_t *instance = list_iterator_next(it);
        if (!strcmp(irc_name(instance), name))
            return instance;
    }
    return NULL;
}

void irc_manager_process(irc_manager_t *manager) {
    list_iterator_t *it = manager->iterator;
    if (!it)
        return;

#ifndef REDROID_USE_EPOLL
    list_iterator_reset(it);
    while (!list_iterator_end(it))
        irc_process(list_iterator_next(it), manager->commander);
#else
    struct epoll_event event;
    memset(&event, 0, sizeof(struct epoll_event));
    if (epoll_wait(manager->epollfd, &event, 1, -1) == -1)
        return;
    irc_process(event.data.ptr, manager->commander);
#endif

    if (!cmd_channel_ready(manager->commander))
        cmd_channel_begin(manager->commander);
    else
        cmd_channel_process(manager->commander);
}


void irc_manager_stage(irc_manager_t *manager) {
#ifdef REDROID_USE_EPOLL
    list_iterator_t *it = manager->iterator;
    if (!it)
        return;

    manager->epollfd = epoll_create(1);

    list_iterator_reset(it);
    while (!list_iterator_end(it)) {
        irc_t *instance = list_iterator_next(it);
        struct epoll_event ev = {
            .data.ptr = instance,
            .events   = EPOLLIN
        };
        epoll_ctl(manager->epollfd, EPOLL_CTL_ADD, instance->sock, &ev);
    }
#endif
}

void irc_manager_add(irc_manager_t *manager, irc_t *instance) {
    if (!manager->iterator)
        manager->iterator = list_iterator_create(manager->instances);

    if (irc_manager_find(manager, irc_name(instance)))
        return;

    list_push(manager->instances, instance);
}
