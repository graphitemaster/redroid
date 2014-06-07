#include <module.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

MODULE_DEFAULT(module);

#define ACCESS 6

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
    "module",
    "access"
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
    irc_write(irc, channel, "%s: module [<-load|-reload|-reload-all|-unload|-enable|-disable|-unload-all|-list-loaded|-list-enabled> [name]]", user);
}

static void mod_load(irc_t *irc, const char *channel, const char *user, const char *module) {
    switch (irc_modules_add(irc, module)) {
        case MODULE_STATUS_FAILURE:
            return irc_write(irc, channel, "%s: failed to load module %s", user, module);
        case MODULE_STATUS_SUCCESS:
            return irc_write(irc, channel, "%s: module %s loaded", user, module);
        case MODULE_STATUS_ALREADY:
            return irc_write(irc, channel, "%s: modules %s is already loaded", user, module);
        default:
            break;
    }
}

static void mod_reload(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!mod_check(irc, channel, user, module, "reload"))
        return;

    switch (irc_modules_reload(irc, module)) {
        case MODULE_STATUS_FAILURE:
            return irc_write(irc, channel, "%s: failed to reload module %s", user, module);
        case MODULE_STATUS_SUCCESS:
            return irc_write(irc, channel, "%s: Ok, module %s reloaded", user, module);
        default:
            break;
    }
}

static void mod_reload_all(irc_t *irc, const char *channel, const char *user) {
    list_t *fail = list_create();
    list_t *list = irc_modules_loaded(irc);
    for (list_iterator_t *it = list_iterator_create(list); !list_iterator_end(it); ) {
        char *name = list_iterator_next(it);
        if (!mod_check(irc, channel, user, name, NULL))
            continue;
        if (irc_modules_reload(irc, name) != MODULE_STATUS_SUCCESS)
            list_push(fail, name);
    }
    irc_write(irc, channel, "%s: Ok, reloaded all modules", user);
    for (list_iterator_t *it = list_iterator_create(fail); !list_iterator_end(it); ) {
        const char *name = list_iterator_next(it);
        irc_write(irc, channel, "%s: module %s couldn't be reloaded", user, name);
    }
}

static void mod_unload(irc_t *irc, const char *channel, const char *user, const char *module, bool force) {
    if (!mod_check(irc, channel, user, module, "unload"))
        return;

    switch (irc_modules_unload(irc, channel, module, force)) {
        case MODULE_STATUS_FAILURE:
            return irc_write(irc, channel, "%s: failed to unload module %s", user, module);
        case MODULE_STATUS_REFERENCED:
            return irc_write(irc, channel, "%s: cannot unload module referenced elsewhere", user, module);
        case MODULE_STATUS_SUCCESS:
            return irc_write(irc, channel, "%s: Ok, module %s unloaded", user, module);
        default:
            break;
    }
}

static void mod_unload_all(irc_t *irc, const char *channel, const char *user) {
    list_t *fail = list_create();
    list_t *list = irc_modules_loaded(irc);
    for (list_iterator_t *it = list_iterator_create(list); !list_iterator_end(it); ) {
        char *name = list_iterator_next(it);
        if (!mod_check(irc, channel, user, name, NULL))
            continue;

        if (irc_modules_unload(irc, channel, name, false) != MODULE_STATUS_SUCCESS)
            list_push(fail, name);
    }
    irc_write(irc, channel, "%s: Ok, unloaded all modules", user);
    for (list_iterator_t *it = list_iterator_create(fail); !list_iterator_end(it); ) {
        const char *name = list_iterator_next(it);
        irc_write(irc, channel, "%s: module %s couldn't be unloaded", user, name);
    }
}

static void mod_list(irc_t *irc, const char *channel, const char *user, bool loaded) {
    list_t          *list   = (loaded) ? irc_modules_loaded(irc) : irc_modules_enabled(irc, channel);
    string_t        *string = string_construct();
    list_iterator_t *it     = list_iterator_create(list);

    while (!list_iterator_end(it)) {
        const char *next = list_iterator_next(it);
        if (!list_iterator_end(it))
            string_catf(string, "%s, ", next);
        else
            string_catf(string, "%s", next);
    }

    irc_write(irc, channel, "%s: modules %s: %s", user, (loaded) ? "loaded" : "enabled", string_contents(string));
}

static void mod_enable(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!mod_check(irc, channel, user, module, "enabled"))
        return;

    switch (irc_modules_enable(irc, channel, module)) {
        case MODULE_STATUS_ALREADY:
            return irc_write(irc, channel, "%s: module %s already enabled", user, module);
        case MODULE_STATUS_FAILURE:
            return irc_write(irc, channel, "%s: module %s enable failed", user, module);
        case MODULE_STATUS_SUCCESS:
            return irc_write(irc, channel, "%s: module %s enabled", user, module);
        case MODULE_STATUS_NONEXIST:
            return irc_write(irc, channel, "%s: module %s not loaded", user, module);
        default:
            break;
    }
}

static void mod_disable(irc_t *irc, const char *channel, const char *user, const char *module) {
    if (!mod_check(irc, channel, user, module, "disable"))
        return;

    switch (irc_modules_disable(irc, channel, module)) {
        case MODULE_STATUS_ALREADY:
            return irc_write(irc, channel, "%s: module %s already disable", user, module);
        case MODULE_STATUS_FAILURE:
            return irc_write(irc, channel, "%s: module %s disable failed", user, module);
        case MODULE_STATUS_SUCCESS:
            return irc_write(irc, channel, "%s: module %s disabled", user, module);
        case MODULE_STATUS_NONEXIST:
            return irc_write(irc, channel, "%s: module %s not loaded", user, module);
        default:
            break;
    }
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return;

    if (!access_check(irc, user, ACCESS))
        return irc_write(irc, channel, "%s: You need access level %d", user, ACCESS);

    list_t     *split  = strnsplit(message, " ", 2);
    const char *method = list_shift(split);
    const char *module = list_shift(split);

    if (!strcmp(method, "-load"))         return mod_load(irc, channel, user, module);
    if (!strcmp(method, "-reload"))       return mod_reload(irc, channel, user, module);
    if (!strcmp(method, "-reload-all"))   return mod_reload_all(irc, channel, user);
    if (!strcmp(method, "-unload"))       return mod_unload(irc, channel, user, module, false);
    if (!strcmp(method, "-unload-force")) return mod_unload(irc, channel, user, module, true);
    if (!strcmp(method, "-unload-all"))   return mod_unload_all(irc, channel, user);
    if (!strcmp(method, "-list-loaded"))  return mod_list(irc, channel, user, true);
    if (!strcmp(method, "-list-enabled")) return mod_list(irc, channel, user, false);
    if (!strcmp(method, "-enable"))       return mod_enable(irc, channel, user, module);
    if (!strcmp(method, "-disable"))      return mod_disable(irc, channel, user, module);
    if (!strcmp(method, "-help"))         return mod_help(irc, channel, user);

    return mod_help(irc, channel, user);
}
