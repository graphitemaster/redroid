#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MODULE_DEFAULT(load);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;

    if (!message)
        return;

    list_iterator_t *it = list_iterator_create(module, irc->modules);
    while (!list_iterator_end(it)) {
        if (!(strcmp(((module_t*)list_iterator_next(it))->name, message)))
            break;
    }

    if (list_iterator_end(it)) {
        // try loading the module from modules/name.so
        string_t *string = string_create(module, "modules/");
        string_catf(string, "%s.so", message);
        if ((irc_modules_add(irc, string_contents(string))))
            irc_write(irc, channel, "loaded module %s", message);
    }
}
