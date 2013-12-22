#include <module.h>
#include <signal.h>

MODULE_DEFAULT(quit);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    raise(SIGINT);
}
