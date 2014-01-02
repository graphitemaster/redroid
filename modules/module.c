#include <module.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

MODULE_DEFAULT(module);

static void mod_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: module [<-load|-reload|-unload|-list> [name]]", user);
}

static void mod_load(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!irc_modules_add(irc, module)) {
        irc_write(irc, channel, "%s: failed to load module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: module %s loaded", user, module);
}

static void mod_reload(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!irc_modules_reload(irc, module)) {
        irc_write(irc, channel, "%s: failed to reload module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: Ok, module %s reloaded", user, module);
}

static void mod_unload(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!irc_modules_unload(irc, module)) {
        irc_write(irc, channel, "%s: failed to unload module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: Ok, module %s unloaded", user, module);
}

static void mod_list(irc_t *irc, const char *channel, const char *user) {
    list_t          *list   = irc_modules_list(irc);
    string_t        *string = string_construct();
    list_iterator_t *it     = list_iterator_create(list);

    while (!list_iterator_end(it)) {
        const char *next = list_iterator_next(it);
        if (!list_iterator_end(it))
            string_catf(string, "%s, ", next);
        else
            string_catf(string, "%s", next);
    }

    irc_write(irc, channel, "%s: modules loaded: %s", user, string_contents(string));
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return;

    if (strstr(message, "-load") == &message[0])
        return mod_load(irc, channel, user, &message[6]);
    else if (strstr(message, "-reload") == &message[0])
        return mod_reload(irc, channel, user, &message[8]);
    else if (strstr(message, "-unload") == &message[0])
        return mod_unload(irc, channel, user, &message[8]);
    else if (strstr(message, "-list") == &message[0])
        return mod_list(irc, channel, user);
    else if (strstr(message, "-help") == &message[0])
        return mod_help(irc, channel, user);
    return mod_help(irc, channel, user);
}
