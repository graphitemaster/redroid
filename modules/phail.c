#include <module.h>

MODULE_DEFAULT(phail);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    irc_write(irc, channel, "%s: Uhuh, graphitemaster iz teh phail.", user);
}

void module_close(irc_t *irc) {
    // nothing
}
