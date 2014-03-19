#include <module.h>
#include <string.h>

MODULE_ALWAYS(dot);

/* The last two are "upside down" versions of the exclamation point and question mark */
static const char dotchars[] = ".!?\xa1\xbf";

static bool pass(const char *string) {
    for (const char *a = string; *a; a++)
        if (!strchr(dotchars, *a))
            return false;
    return true;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (message && pass(message)) {
        string_t *reply = string_format("%s%c", message, dotchars[urand() % (sizeof(dotchars) - 1)]);
        irc_write(irc, channel, string_contents(reply));
    }
}
