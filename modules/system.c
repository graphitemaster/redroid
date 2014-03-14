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
    if (!strcmp(message, "-recompile"))
        return redroid_recompile(irc, channel, user);

    if (!strcmp(message, "-test-timeout"))
        for (;;) ;
    if (!strcmp(message, "-test-crash"))
        *((int*){0}) = 0;

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

    if (!strcmp(message, "-users")) {
        string_t *string = string_construct();
        list_t   *users  = irc_users(irc, channel);

        for (list_iterator_t *it = list_iterator_create(users); !list_iterator_end(it); ) {
            char *copy = strdup(list_iterator_next(it));
            copy[urand() % strlen(copy)] = '*';
            if (!list_iterator_end(it))
                string_catf(string, "%s, ", copy);
            else
                string_catf(string, "%s", copy);
        }

        irc_write(irc, channel, "%s: to preventing highlighting the nicks will contain '*'", user);
        irc_write(irc, channel, "%s: %s", user, string_contents(string));
        return;
    }

    if (!strcmp(message, "-topic")) {
        irc_write(irc, channel, "%s: %s", user, irc_topic(irc, channel));
        return;
    }

help:
    irc_write(irc, channel, "%s: system <-shutdown|-restart|-recompile|-test-timeout|-test-crash|-topic|-version>|<-join|-part> <channel>", user);
}
