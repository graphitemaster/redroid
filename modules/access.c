#include <module.h>
#include <stdlib.h>
#include <string.h>

#define ACCESS_CONTROL 4

MODULE_DEFAULT(access);

static void access_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: access [-add|-change|-forget] <nick> <-2|1|0|1|2|3|4|5|6>", user);
    irc_write(irc, channel, "%s: -2 = ban user on entry (shitlist), -1 = bot ignores user", user);
}

static void access_add(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    switch (access_insert(irc, target, invoke, level)) {
        case ACCESS_NOEXIST_INVOKE:
        case ACCESS_DENIED:
            return irc_write(irc, channel, "%s: You need access level %d or higher", invoke, ACCESS_CONTROL);
        case ACCESS_SUCCESS:
            return irc_write(irc, channel, "%s: Ok, added %s with level %d", invoke, target, level);
        case ACCESS_EXISTS:
            return irc_write(irc, channel, "%s: %s already exists", invoke, target);
        default:
            return access_help(irc, channel, invoke);
    }
}

static void access_modify(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    int targetlevel = 0;
    switch (access_change(irc, target, invoke, level)) {
        case ACCESS_NOEXIST_TARGET:
            return irc_write(irc, channel, "%s: %s doesn't exist", invoke, target);
        case ACCESS_NOEXIST_INVOKE:
        case ACCESS_DENIED:
            access_level(irc, target, &targetlevel);
            return irc_write(irc, channel, "%s: You need access level %d or higher", invoke, targetlevel);
        case ACCESS_SUCCESS:
            return irc_write(irc, channel,
                "%s: Ok, %s %s access level %d now",
                invoke,
                (!strcmp(invoke, target)) ? "you"  : target,
                (!strcmp(invoke, target)) ? "have" : "has",
                level
            );
        default:
            return access_help(irc, channel, invoke);
    }
}

static void access_forget(irc_t *irc, const char *channel, const char *target, const char *invoke) {
    int targetlevel = 0;
    switch (access_remove(irc, target, invoke)) {
        default:
            return access_help(irc, channel, invoke);
        case ACCESS_NOEXIST_TARGET:
            return irc_write(irc, channel, "%s: %s doesn't exist", invoke, target);
        case ACCESS_DENIED:
            access_level(irc, target, &targetlevel);
            return irc_write(irc, channel, "%s: You need access level %d or higher", invoke, targetlevel);
        case ACCESS_SUCCESS:
            return irc_write(irc, channel, "%s: Ok, removed %s", invoke, target);
    }
}

static void access_user(irc_t *irc, const char *channel, const char *target, const char *invoke) {
    int level = 0;
    access_level(irc, target, &level);
    irc_write(irc, channel, "%s: %s %s access level %d", invoke,
        (!strcmp(invoke, target)) ? "You" : target,
        (!strcmp(invoke, target)) ? "have": "has",
        level
    );
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t     *list        = strnsplit(message, " ", 3);
    const char *method      = list_shift(list);
    const char *target      = list_shift(list);
    const char *targetlevel = list_shift(list);

    if (!method || !strcmp(method, "-help"))
        return access_help(irc, channel, user);

    if (!strcmp(method, "-add")) {
        if (!target || !targetlevel)
            return access_help(irc, channel, user);
        return access_add(irc, channel, target, user, atoi(targetlevel));
    } else if (!strcmp(method, "-change")) {
        if (!target || !targetlevel)
            return access_help(irc, channel, user);
        return access_modify(irc, channel, target, user, atoi(targetlevel));
    } else if (!strcmp(method, "-forget"))
        return access_forget(irc, channel, target, user);
    return access_user(irc, channel, method, user);
}
