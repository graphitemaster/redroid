#include <module.h>

MODULE_DEFAULT(dns);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    (void)irc;
    (void)channel;
    (void)user;
    (void)message;
}
