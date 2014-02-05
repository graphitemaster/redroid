#include <module.h>
#include <string.h>
#include <ctype.h>

MODULE_DEFAULT(system);

#define empty(X) (!(X) || !*(X))

static const char *next(const char *forward, const char ch) {
    char *input = strchr(forward, ch);
    if (!input || !*input)
        return NULL;

    while (*input && isspace(*input))
        input++;

    return input;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        goto help;

    /*
     * TODO: check if user is an administrator and can perform the
     *       restart.
     */
    if (!strcmp(message, "-shutdown"))
        return redroid_shutdown(irc, channel, user);
    if (!strcmp(message, "-restart"))
        return redroid_restart(irc, channel, user);

    if (!strcmp(message, "-timeout"))
        for (;;) ;

    if (!strncmp(message, "-join", 5)) {
        const char *peek = next(message, ' ');
        if (empty(peek))
            goto help;
        irc_join(irc, peek);
        irc_write(irc, channel, "%s: Ok, joined channel %s", user, peek);
        return;
    }

    if (!strncmp(message, "-part", 5)) {
        const char *peek = next(message, ' ');
        if (empty(peek)) {
            irc_write(irc, channel, "%s: Ok, parting this channel", user);
            irc_part(irc, channel);
            return;
        }
        irc_part(irc, peek);
        irc_write(irc, channel, "%s: Ok, parted channel %s", user, peek);
        return;
    }

    if (!strcmp(message, "-version")) {
        irc_write(irc, channel, "%s: %s", user, build_version());
        return;
    }

help:
    irc_write(irc, channel, "%s: system <-shutdown|-restart|-timeout|-version>|<-join|-part> <channel>", user);
}
