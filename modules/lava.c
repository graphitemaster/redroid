#include <module.h>

MODULE_DEFAULT(lava);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;

    if (message)
        irc_write(irc, channel, "%s melted in a ball of flames", message);
    else
        irc_write(irc, channel, "%s melted in blinding agony", user);
}
