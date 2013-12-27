#include <module.h>
#include <string.h>
#include <stdlib.h>

MODULE_DEFAULT(fail);

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc = module->instance;

    if (!message)
        return;

    if (!strcmp(message, irc->nick))
        irc_write(irc, channel, "%s: Nuh-uh, you are teh fail for even thinking I could be.", user);
    else if (rand() % 3 == rand() % 2)
        irc_write(irc, channel, "%s: Nopez, %s seems to be teh win.", user, message);
    else
        irc_write(irc, channel, "%s: Uhuh, %s iz teh fail.", user, message);
}