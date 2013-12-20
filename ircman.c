#include "list.h"
#include "ircman.h"

#include <stdlib.h> // malloc, free
#include <string.h> // strcmp

struct irc_manager_s {
    list_t          *instances;
    list_iterator_t *iterator; // cached iterator
};

irc_manager_t *irc_manager_create(void) {
    irc_manager_t *man = malloc(sizeof(*man));
    if (!man)
        return NULL;

    if (!(man->instances = list_create())) {
        free(man);
        return NULL;
    }

    man->iterator = NULL;
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
    if (!it) // only if we have a list to process
        return;

    list_iterator_reset(it);
    while (!list_iterator_end(it))
        irc_process(list_iterator_next(it));
}

void irc_manager_add(irc_manager_t *manager, irc_t *instance) {
    if (!manager->iterator)
        manager->iterator = list_iterator_create(manager->instances);

    if (irc_manager_find(manager, irc_name(instance)))
        return;

    list_push(manager->instances, instance);
}
