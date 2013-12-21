#include <module.h>

MODULE_DEFAULT(dance);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        irc_action(irc, channel, "dances like a jolly idiot");
    else
        // TODO: user list checl
        irc_action(irc, channel, "gives %s a lap dance - oooh-lah-lah", message);
}

void module_close(irc_t *irc) {
    // Nothing
}
