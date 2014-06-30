#include <module.h>
#include <stdlib.h>

MODULE_DEFAULT(dur);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message) {
        irc_write(irc, channel, "%s: ?\n", user);
        return;
    }

    unsigned long long num = strtoull(message, NULL, 0);
    irc_write(irc, channel, "%s: %s", user, strdur(num));
}
