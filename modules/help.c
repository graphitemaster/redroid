#include <module.h>

MODULE_DEFAULT(help);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_write(module->instance, channel, "%s: Sorry, I don't think there's any help for you at all..", user);
}



