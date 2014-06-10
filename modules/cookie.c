#include <module.h>
#include <string.h>

MODULE_DEFAULT(cookie);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    const char *target = NULL;
    if (message) {
        target = list_search(irc_users(irc, channel), message,
            lambda bool(const char *a, const char *b) {
                 return !strcmp(a, b);
            }
        );
    }
    irc_action(irc, channel, "chops %s up and makes cookies out of the pieces",
        target ? target : user);
}
