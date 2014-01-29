#include <module.h>

MODULE_DEFAULT(restart);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    restart(irc, channel, user);
}
