#include <module.h>
#include <stdlib.h>
#include <string.h>

MODULE_DEFAULT(reload);

static void reload_all(module_t *module, const char *channel) {
    list_iterator_t *it = list_iterator_create(module, module->instance->modules);
    while (!list_iterator_end(it))
        module_reload(list_iterator_next(it));
    irc_write(module->instance, channel, "reloaded all modules");
}

static void reload_one(module_t *module, const char *channel, const char *name) {
    list_iterator_t *it = list_iterator_create(module, module->instance->modules);
    while (!list_iterator_end(it)) {
        module_t *module = list_iterator_next(it);
        if (!strcmp(module->name, name)) {
            module_reload(module);
            break;
        }
    }
    if (!list_iterator_end(it))
        irc_write(module->instance, channel, "reloaded module %s", name);
}

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    if (!message)
        reload_all(module, channel);
    else
        reload_one(module, channel, message);
}
