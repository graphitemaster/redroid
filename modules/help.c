#include <module.h>

MODULE_DEFAULT(help);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    (void)message; /* unused */
    irc_write(irc, channel, "%s: Sorry, I don't think there's any help for you at all..", user);
}



