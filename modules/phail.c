#include <module.h>

MODULE_DEFAULT(phail);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;
    irc_write(irc, channel, "%s: Uhuh, graphitemaster iz teh phail.", user);
}
