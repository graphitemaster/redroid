#include <module.h>
#include <signal.h>
#include <string.h>

MODULE_DEFAULT(quit);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;
    if (strcmp(user, "graphitemaster"))
        irc_write(irc, channel, "%s: sorry, you're not allowed to quit", user);
    else
        raise(SIGINT);
}
