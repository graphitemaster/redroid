#include <module.h>
#include <string.h>
#include <stdlib.h>

MODULE_DEFAULT(access);

static void access_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: access [-add|-change|-forget] <nick> <-2|1|0|1|2|3|4|5|6>", user);
    irc_write(irc, channel, "%s: -2 = ban user on entry (shitlist), -1 = bot ignores user", user);
}

static void access_add(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    switch (access_insert(irc, channel, target, invoke, level)) {
        case ACCESS_NOEXIST_INVOKE:
        case ACCESS_DENIED:
            irc_write(irc, channel, "%s: You need access level %d or higher", invoke, ACCESS_CONTROL);
            return;
        case ACCESS_SUCCESS:
            irc_write(irc, channel, "%s: Ok, added %s with level %d", invoke, target, level);
            return;
        case ACCESS_EXISTS:
            irc_write(irc, channel, "%s: %s already exists", invoke, target);
            return;
        case ACCESS_FAILED:
            return access_help(irc, channel, invoke);
    }
}

static void access_modify(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    int targetlevel = 0;
    switch (access_change(irc, channel, target, invoke, level)) {
        case ACCESS_NOEXIST_TARGET:
            irc_write(irc, channel, "%s: %s doesn't exist", invoke, target);
            return;
        case ACCESS_NOEXIST_INVOKE:
        case ACCESS_DENIED:
            access_level(irc, channel, target, &targetlevel);
            irc_write(irc, channel, "%s: You need access level %d or higher", invoke, targetlevel);
            return;
        case ACCESS_SUCCESS:
            irc_write(irc, channel,
                "%s: Ok, %s %s access level %d now",
                invoke,
                (!strcmp(invoke, target)) ? "you"  : target,
                (!strcmp(invoke, target)) ? "have" : "has",
                level
            );
    }
}

static void access_forget(irc_t *irc, const char *channel, const char *target, const char *invoke) {
    int targetlevel = 0;
    switch (access_remove(irc, channel, target, invoke)) {
        case ACCESS_FAILED:
        case ACCESS_NOEXIST_INVOKE:
            return access_help(irc, channel, invoke);
        case ACCESS_NOEXIST_TARGET:
            irc_write(irc, channel, "%s: %s doesn't exist", invoke, target);
            return;
        case ACCESS_DENIED:
            access_level(irc, channel, target, &targetlevel);
            irc_write(irc, channel, "%s: You need access level %d or higher", targetlevel);
            return;
        case ACCESS_SUCCESS:
            irc_write(irc, channel, "%s: Ok, removed %s", invoke, target);
            return;
    }
}

static void access_user(irc_t *irc, const char *channel, const char *target, const char *invoke) {
    int level = 0;
    access_level(irc, channel, target, &level);
    irc_write(irc, channel, "%s: %s %s access level %d", invoke,
        (!strcmp(invoke, target)) ? "You" : target,
        (!strcmp(invoke, target)) ? "have": "has",
        level
    );
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t     *list       = strnsplit(message, " ", 3);
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
