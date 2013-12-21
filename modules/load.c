#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MODULE_DEFAULT(load);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return;

    list_iterator_t *it = list_iterator_create(irc->modules);
    while (!list_iterator_end(it)) {
        if (!(strcmp(((module_t*)list_iterator_next(it))->name, message)))
            break;
    }
    if (list_iterator_end(it)) {
        // try loading the module from modules/name.so
        char *load = NULL;
        asprintf(&load, "modules/%s.so", message);
        if ((irc_modules_add(irc, load)))
            irc_write(irc, channel, "loaded module %s", message);
        free(load);
    }
    list_iterator_destroy(it);
}

void module_close(irc_t *irc) {
    // nothing
}
