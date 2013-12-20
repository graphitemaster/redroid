#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include <stdbool.h> // bool, true, false

typedef struct irc_s irc_t;

irc_t *irc_create(const char *name, const char *nick, const char *pattern);
void  irc_destroy(irc_t *irc);
int irc_process(irc_t *irc);
const char *irc_name(irc_t *irc);

int irc_connect(irc_t *irc, const char *host, const char *port);

bool irc_modules_add(irc_t *irc, const char *file);
bool irc_channels_add(irc_t *irc, const char *channel);
void (*irc_modules_command(irc_t *irc, const char *command))(irc_t *irc, const char *channel, const char *nick, const char *message);

#ifdef __cplusplus
extern "C" {
#endif /*! __cplusplus */

extern int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
extern int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);

#ifdef __cplusplus
}
#endif /*! __cplusplus */

#endif
