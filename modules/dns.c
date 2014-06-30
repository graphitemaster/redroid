#include <module.h>

MODULE_DEFAULT(dns);

typedef struct {
    irc_t      *irc;
    const char *channel;
    const char *user;
} pass_t;

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    (void)irc;
    (void)channel;
    (void)user;
    (void)message;

    irc_write(irc, channel, "%s: Please wait, resolving: %s", user, message);

    list_t *result = dns(message);

    if (result) {
        pass_t pass = {
            .irc     = irc,
            .channel = channel,
            .user    = user
        };

        list_foreach(result, &pass,
            lambda void(const char *resolved, pass_t *pass)
                => irc_write(pass->irc, pass->channel, "%s: %s", pass->user, resolved);
        );
    } else {
        irc_write(irc, channel, "%s: domain name resolution failed (%s)", user, message);
    }
}
