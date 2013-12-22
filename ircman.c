#include "list.h"
#include "ircman.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>

struct irc_manager_s {
    list_t          *instances;
    list_iterator_t *iterator; // cached iterator
    cmd_pool_t      *commander;
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
    man->commander = cmd_pool_create();
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

    cmd_pool_destroy(manager->commander);
    list_destroy(manager->instances);
    free(manager);
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

    list_iterator_reset(it);
    while (!list_iterator_end(it))
        irc_process(list_iterator_next(it), manager->commander);

    if (!cmd_pool_ready(manager->commander))
        cmd_pool_begin(manager->commander);
    else
        cmd_pool_process(manager->commander);
}

void irc_manager_add(irc_manager_t *manager, irc_t *instance) {
    if (!manager->iterator)
        manager->iterator = list_iterator_create(manager->instances);

    if (irc_manager_find(manager, irc_name(instance)))
        return;

    list_push(manager->instances, instance);
}
