#include <module.h>
#include <string.h>

MODULE_DEFAULT(system);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        goto help;

    if (!strcmp(message, "-shutdown")) {
        /*
         * TODO: check if user is an administrator and can perform the
         *       shutdown.
         */
        return redroid_shutdown(irc, channel, user);
    }

    if (!strcmp(message, "-restart")) {
        /*
         * TODO: check if user is an administrator and can perform the
         *       restart.
         */
        return redroid_restart(irc, channel, user);
    }

    if (!strcmp(message, "-version")) {
        irc_write(irc, channel, "%s: %s", user, build_version());
        return;
    }

help:
    irc_write(irc, channel, "%s: system <-shutdown|-restart|-version>", user);
}
