#include <module.h>
#include <stdlib.h>
#include <stdio.h>

MODULE_DEFAULT(modules);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t          *modules  = irc->modules;
    list_iterator_t *it       = NULL;
    char            *buffer   = NULL;
    char            *contents = NULL;

    for (it = list_iterator_create(modules); !list_iterator_end(it); ) {
        module_t *module = list_iterator_next(it);
        if (contents) {
            asprintf(&buffer, "%s, %s", contents, module->name);
            free(contents);
            contents = buffer;
        } else {
            asprintf(&buffer, "%s", module->name);
            contents = buffer;
        }
    }

    irc_write(irc, channel, "%s: loaded modules: %s", user, contents);
    free(contents);
    list_iterator_destroy(it);
}
