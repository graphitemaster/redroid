#include <module.h>

MODULE_DEFAULT(cookie);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    void *leak = module_alloc(module, 1024);
    irc_t *irc = module->instance;
    const char *target = (message) ? message : user;
    // TODO: channel user list
    irc_action(irc, channel, "chops %s up and makes cookies out of the pieces", target);
}
