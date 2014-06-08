#include <module.h>
#include <string.h>

MODULE_DEFAULT(cookie);

static bool cookie_user(const void *a, const void *b) {
    return !strcmp((const char *)a, (const char *)b);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    const char *target;
    if (!message) {
        target = user;
        goto end;
    }

    list_t *users = irc_users(irc, channel);
    target = (list_search(users, message, &cookie_user))
                             ? message
                             : user;

end:
    irc_action(irc, channel, "chops %s up and makes cookies out of the pieces", target);
}
