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
        *((volatile int*){0}) = 0; /* volatile to prevent compiler from optimizing it away */
    if (!strcmp(message, "-test-large-payload")) {
        irc_write(irc, channel,
            "%s: Lorem ipsum dolor sit amet, consectetuer adipiscing elit, "
            "sed diam nonummy nibh euismod tincidunt ut laoreet dolore magna "
            "aliquam erat volutpat. Ut wisi enim ad minim veniam, quis nostrud "
            "exerci tation ullamcorper suscipit lobortis nisl ut aliquip ex ea "
            "commodo consequat. Duis autem vel eum iriure dolor in hendrerit in "
            "vulputate velit esse molestie consequat, vel illum dolore eu feugiat "
            "nulla facilisis at vero eros et accumsan et iusto odio dignissim qui "
            "blandit praesent luptatum zzril delenit augue duis dolore te feugait "
            "nulla facilisi. Nam liber tempor cum soluta nobis eleifend option "
            "congue nihil imperdiet doming id quod mazim placerat facer possim "
            "assum. Typi non habent claritatem insitam; est usus legentis in iis "
            "qui facit eorum claritatem. Investigationes demonstraverunt lectores "
            "legere me lius quod ii legunt saepius. Claritas est etiam processus "
            "dynamicus, qui sequitur mutationem consuetudium lectorum. Mirum est "
            "notare quam littera gothica, quam nunc putamus parum claram, "
            "anteposuerit litterarum formas humanitatis per seacula quarta "
            "decima et quinta decima. Eodem modo typi, qui nunc nobis videntur "
            "parum clari, fiant sollemnes in futurum.", user);
        return;
    }

    if (!strncmp(message, "-join", 5)) {
        const char *peek = next(message, ' ');
        if (empty(peek))
            goto help;
        irc_join(irc, peek);
        irc_write(irc, channel, "%s: Ok, joined channel %s", user, peek);
        return;
    }

    if (!strcmp(message, "-part-all")) {
        irc_write(irc, channel, "%s: parting all but this channel", user);
        list_t *channels = irc_channels(irc);
        for (list_iterator_t *it = list_iterator_create(channels); !list_iterator_end(it); ) {
            const char *name = list_iterator_next(it);
            if (!strcmp(name, channel))
                continue;
            irc_part(irc, name);
        }
        irc_write(irc, channel, "%s: Ok, parted all but this channel", user);
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

    if (!strcmp(message, "-channels")) {
        string_t *string   = string_construct();
        list_t   *channels = irc_channels(irc);

        for (list_iterator_t *it = list_iterator_create(channels); !list_iterator_end(it); ) {
            const char *channel = list_iterator_next(it);
            if (!list_iterator_end(it))
                string_catf(string, "%s, ", channel);
            else
                string_catf(string, "%s", channel);
        }

        irc_write(irc, channel, "%s: %s", user, string_contents(string));
        return;
    }

    if (!strcmp(message, "-topic")) {
        irc_write(irc, channel, "%s: %s", user, irc_topic(irc, channel));
        return;
    }

help:
    irc_write(irc, channel, "%s: system <-shutdown|-restart|-recompile|-test-timeout|-test-crash|-topic|-version|-part-all|-users|-channels>|<-join|-part> <channel>", user);
}
