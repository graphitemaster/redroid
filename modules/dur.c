#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

MODULE_DEFAULT(dur);

static const char dur_index[] = "wdhms";
static const char dur_length = sizeof(dur_index) - 1;
static const unsigned long dur_seconds[] = {
    60*60*24*7,
    60*60*24,
    60*60,
    60,
    1
};

static bool dur_check(const char *str) {
    for (const char *s = str; *s; s++)
        if (!isdigit(*s))
            return false;
    return true;
}

static void dur_forward(irc_t *irc, const char *channel, const char *user, const char *message) {
    unsigned long long duration = strtoull(message, NULL, 0);
    const char *convert = strdur(duration);
    irc_write(irc, channel, "%s: %s", user, convert);
}

static void dur_help(irc_t *irc, const char *channel, const char *user) {
    return irc_write(irc, channel, "%s: ?\n", user);
}

static void dur_reverse(irc_t *irc, const char *channel, const char *user, const char *message) {
    unsigned char bits = 0;
    unsigned long long accumulate = 0;
    for (const char *str = message; *str; str++) {
        unsigned long read = 0;
        char *find;
        while (isdigit(*str))
            read = (read << 3) + (read << 1) + *str++ - '0';
        if ((find = strchr(dur_index, *str))) {
            size_t shift = find - dur_index;
            if (bits & (1 << shift))
                return irc_write(irc, channel, "%s: `%c' already specified in duration string", user, *find);
            bits |= 1 << shift;
            accumulate += dur_seconds[shift] * read;
        } else {
            return dur_help(irc, channel, user);
        }
    }
    irc_write(irc, channel, "%s: %llu", user, accumulate);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return dur_help(irc, channel, user);
    if (dur_check(message))
        return dur_forward(irc, channel, user, message);
    return dur_reverse(irc, channel, user, message);
}
