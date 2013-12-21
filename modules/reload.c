#include <module.h>
#include <string.h>
#include <stdlib.h>

MODULE_DEFAULT(reload);

static void reload_all(irc_t *irc, const char *channel) {
    list_iterator_t *it = list_iterator_create(irc->modules);
    while (!list_iterator_end(it))
        module_reload(list_iterator_next(it));
    list_iterator_destroy(it);
    irc_write(irc, channel, "reloaded all modules");
}

static void reload_one(irc_t *irc, const char *channel, const char *name) {
    list_iterator_t *it = list_iterator_create(irc->modules);
    while (!list_iterator_end(it)) {
        module_t *module = list_iterator_next(it);
        if (!strcmp(module->name, name)) {
            module_reload(module);
            break;
        }
    }
    if (!list_iterator_end(it))
        irc_write(irc, channel, "reloaded module %s", name);
    list_iterator_destroy(it);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message || !strlen(message))
        reload_all(irc, channel);
    else
        reload_one(irc, channel, message);
}

void module_close(irc_t *irc) {
    // nothing
}
