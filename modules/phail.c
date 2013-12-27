#include <module.h>

MODULE_DEFAULT(phail);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;
    irc_write(irc, channel, "%s: Nuh-uh, you are teh fail for even thinking graphitemaster could phail.", user);
}
