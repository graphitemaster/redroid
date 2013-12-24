#include <module.h>

MODULE_DEFAULT(nick);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!strcmp(user, "graphitemaster"))
        sock_sendf(irc->sock, "NICK %s\r\n", message);
    else
        irc_write(irc, channel, "%s: sorry you don't have permission to do that", user);
}
