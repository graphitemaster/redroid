#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include <stdbool.h> // bool, true, false

typedef struct irc_s irc_t;

irc_t *irc_create(const char *name, const char *nick);
void  irc_destroy(irc_t *irc);
int irc_process(irc_t *irc);
const char *irc_name(irc_t *irc);

int irc_connect(irc_t *irc, const char *host, const char *port);

bool irc_modules_add(irc_t *irc, const char *file);

#endif
