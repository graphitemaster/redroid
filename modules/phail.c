#include <module.h>

MODULE_DEFAULT(phail);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    (void)message; /* unused */
    irc_write(irc, channel, "%s: Nuh-uh, you are teh fail for even thinking graphitemaster could phail.", user);
}
