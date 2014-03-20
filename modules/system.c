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

static void system_test_timeout(void) {
    for (;;) ;
}

static void system_test_crash(void) {
     /* volatile to prevent compiler from optimizing it away */
    *((volatile int*){0}) = 0;
}

static void system_test_large_payload(irc_t *irc, const char *channel, const char *user) {
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
        "parum clari, fiant sollemnes in futurum.",
        user
    );
}

static void system_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel,
        "%s: system <-shutdown|-restart|-recompile|-test-timeout|-test-crash|"
        "-topic|-version|-part-all|-users|-channels>|<-join|-part> <channel>",
        user
    );
}

static void system_join(irc_t *irc, const char *channel, const char *user, const char *join) {
    irc_join(irc, join);
    irc_write(irc, channel, "%s: Ok, joined channel %s", user, join);
}

static void system_part(irc_t *irc, const char *channel, const char *user, const char *part) {
    if (!part) {
        irc_write(irc, channel, "%s: Ok, parting this channel", user);
        irc_part(irc, channel);
    } else {
        irc_part(irc, part);
        irc_write(irc, channel, "%s: Ok, parted channel %s", user, part);
    }
}

static void system_part_all(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: parting all but this channel", user);
    list_t *channels = irc_channels(irc);
    for (list_iterator_t *it = list_iterator_create(channels); !list_iterator_end(it); ) {
        const char *name = list_iterator_next(it);
        if (!strcmp(name, channel))
            continue;
        irc_part(irc, name);
    }
    irc_write(irc, channel, "%s: Ok, parted all but this channel", user);
}

static void system_version(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: %s", user, build_version());
}

static void system_users(irc_t *irc, const char *channel, const char *user) {
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
}

static void system_channels(irc_t *irc, const char *channel, const char *user) {
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
}

static void system_topic(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: %s", user, irc_topic(irc, channel));
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t     *list   = strnsplit(message, " ", 2);
    const char *method = list_shift(list);

    if (!method || !strcmp(method, "-help"))
        return system_help(irc, channel, user);

    if (!strcmp(method, "-shutdown"))           return redroid_shutdown(irc, channel, user);
    if (!strcmp(method, "-restart"))            return redroid_restart(irc, channel, user);
    if (!strcmp(method, "-recompile"))          return redroid_recompile(irc, channel, user);
    if (!strcmp(method, "-test-timeout"))       return system_test_timeout();
    if (!strcmp(method, "-test-crash"))         return system_test_crash();
    if (!strcmp(method, "-test-large-payload")) return system_test_large_payload(irc, channel, user);
    if (!strcmp(method, "-join"))               return system_join(irc, channel, user, list_shift(list));
    if (!strcmp(method, "-part"))               return system_part(irc, channel, user, list_shift(list));
    if (!strcmp(method, "-part-all"))           return system_part_all(irc, channel, user);
    if (!strcmp(method, "-version"))            return system_version(irc, channel, user);
    if (!strcmp(method, "-users"))              return system_users(irc, channel, user);
    if (!strcmp(method, "-channels"))           return system_channels(irc, channel, user);
    if (!strcmp(method, "-topic"))              return system_topic(irc, channel, user);

    return system_help(irc, channel, user);
}
