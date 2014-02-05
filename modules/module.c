#include <module.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

MODULE_DEFAULT(module);

/*
 * A list of modules which cannot be unloaded/reloaded
 *
 *  There is no reason the modules within this list cannot be unloaded,
 *  or reloaded. It's just a design decision to deal with the problems
 *  which could occur if they could be unloaded/reloaded.
 *
 *  Without the system module there is just no way to restart Redroid
 *  from within IRC. This is potentially bad.
 *
 *  Unloading the module module will make it impossible to load or unload
 *  future modules. That being said it's possible to get the module
 *  module back if the bot was restarted with the system module.
 *
 *  But if someone were to do something like the following:
 *      ~module -unload system
 *      ~module -unload module
 *
 *  Then it's a check mate. There is simply no way to restart the bot or
 *  get the module module back. A clever person could do more damage of
 *  course if they did something like:
 *      ~module -unload-all
 *
 *  Then the bot is simply useless. No modules will be running thus the
 *  bot defunct. So to prevent these situations we simply prevent these
 *  modules from ever being unloaded/reloaded.
 *
 *  One would argue that reloading of modules has a semantic binding that
 *  they'll still exist after reloading. Typically that'd be a correct
 *  assumption if it weren't for the fact that a module can fail to
 *  reload.
 *
 *  That being said if these modules do need to be reloaded you can
 *  have them reloaded by invoking ~system -restart. This will restart
 *  the bot and essentially reload everything.
 */
static const char *mod_mask[] = {
    "system",
    "module"
};

static bool mod_check(irc_t *irc, const char *channel, const char *user, const char *module, const char *from) {
    for (size_t i = 0; i < sizeof(mod_mask)/sizeof(*mod_mask); i++) {
        if (!strcmp(module, mod_mask[i])) {
            if (from)
                irc_write(irc, channel, "%s: module %s cannot be %sed", user, module, from);
            return false;
        }
    }
    return true;
}

static void mod_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: module [<-load|-reload|-reload-all|-unload|-unload-all|-list> [name]]", user);
}

static void mod_load(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!irc_modules_add(irc, module)) {
        irc_write(irc, channel, "%s: failed to load module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: module %s loaded", user, module);
}

static void mod_reload(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!mod_check(irc, channel, user, module, "reload"))
        return;

    if (!irc_modules_reload(irc, module)) {
        irc_write(irc, channel, "%s: failed to reload module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: Ok, module %s reloaded", user, module);
}

static void mod_reload_all(irc_t *irc, const char *channel, const char *user) {
    list_t *fail = list_create();
    list_t *list = irc_modules_list(irc);
    for (list_iterator_t *it = list_iterator_create(list); !list_iterator_end(it); ) {
        char *name = list_iterator_next(it);
        if (!mod_check(irc, channel, user, name, NULL))
            continue;
        if (!irc_modules_reload(irc, name))
            list_push(fail, name);
    }
    irc_write(irc, channel, "%s: Ok, reloaded all modules", user);
    for (list_iterator_t *it = list_iterator_create(fail); !list_iterator_end(it); ) {
        const char *name = list_iterator_next(it);
        irc_write(irc, channel, "%s: module %s couldn't be reloaded", user, name);
    }
}

static void mod_unload(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!mod_check(irc, channel, user, module, "unload"))
        return;

    if (!irc_modules_unload(irc, module)) {
        irc_write(irc, channel, "%s: failed to unload module %s", user, module);
        return;
    }
    irc_write(irc, channel, "%s: Ok, module %s unloaded", user, module);
}

static void mod_unload_all(irc_t *irc, const char *channel, const char *user) {
    list_t *fail = list_create();
    list_t *list = irc_modules_list(irc);
    for (list_iterator_t *it = list_iterator_create(list); !list_iterator_end(it); ) {
        char *name = list_iterator_next(it);
        if (!mod_check(irc, channel, user, name, NULL))
            continue;

        if (!irc_modules_unload(irc, name))
            list_push(fail, name);
    }
    irc_write(irc, channel, "%s: Ok, unloaded all modules", user);
    for (list_iterator_t *it = list_iterator_create(fail); !list_iterator_end(it); ) {
        const char *name = list_iterator_next(it);
        irc_write(irc, channel, "%s: module %s couldn't be unloaded", user, name);
    }
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

    list_t     *split  = strnsplit(message, " ", 2);
    const char *method = list_shift(split);
    const char *module = list_shift(split);

    if (!strcmp(method, "-load"))       return mod_load(irc, channel, user, module);
    if (!strcmp(method, "-reload"))     return mod_reload(irc, channel, user, module);
    if (!strcmp(method, "-reload-all")) return mod_reload_all(irc, channel, user);
    if (!strcmp(method, "-unload"))     return mod_unload(irc, channel, user, module);
    if (!strcmp(method, "-unload-all")) return mod_unload_all(irc, channel, user);
    if (!strcmp(method, "-list"))       return mod_list(irc, channel, user);
    if (!strcmp(method, "-help"))       return mod_help(irc, channel, user);

    return mod_help(irc, channel, user);
}
