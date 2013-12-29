#include <module.h>
#include <signal.h>
#include <string.h>

MODULE_DEFAULT(quit);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (strcmp(user, "graphitemaster")) {
        irc_write(irc, channel, "%s: sorry, you're not allowed to quit", user);
        return;
    }
    raise(SIGUSR1); // safe kill
}
