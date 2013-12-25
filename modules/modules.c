#include <module.h>
#include <stdlib.h>
#include <stdio.h>

MODULE_DEFAULT(modules);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t           *irc      = module->instance;
    list_t          *modules  = irc->modules;
    list_iterator_t *it       = NULL;
    string_t        *list     = string_construct();

    for (it = list_iterator_create(modules); !list_iterator_end(it); ) {
        module_t *module = list_iterator_next(it);
        string_catf(list, "%s", module->name);
        if (!list_iterator_end(it))
            string_catf(list, ", ");
    }

    irc_write(irc, channel, "%s: loaded modules: %s", user, string_contents(list));
    string_destroy(list);
    list_iterator_destroy(it);
}
