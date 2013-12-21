#include <module.h>

MODULE_DEFAULT(cookie);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    const char *target = (message) ? message : user;
    // TODO: channel user list
    irc_action(irc, channel, "chops %s up and makes cookies out of the pieces", target);
}

void module_close(irc_t *irc) {
    // nothing
}
