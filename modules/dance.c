#include <module.h>
#include <string.h>

MODULE_DEFAULT(dance);

static bool dance_user(const void *a, const void *b) {
    return !strcmp((const char *)a, (const char *)b);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message || !list_search(irc_users(irc, channel), &dance_user, message))
        irc_action(irc, channel, "dances like a jolly idiot");
    else
        irc_action(irc, channel, "gives %s a lap dance - oooh-lah-lah", message);
}
