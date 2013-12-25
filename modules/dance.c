#include <module.h>
#include <string.h>

MODULE_DEFAULT(dance);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;
    if (!message)
        irc_action(irc, channel, "dances like a jolly idiot");
    else
        // TODO: user list check
        irc_action(irc, channel, "gives %s a lap dance - oooh-lah-lah", message);
}
