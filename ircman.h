#ifndef REDROID_IRCMAN_HDR
#define REDROID_IRCMAN_HDR
#include "irc.h"

typedef struct irc_manager_s irc_manager_t;

irc_manager_t *irc_manager_create(void);
void irc_manager_destroy(irc_manager_t *manager);
irc_t *irc_manager_find(irc_manager_t *manager, const char *name);
void irc_manager_process(irc_manager_t *manager);
void irc_manager_add(irc_manager_t *manager, irc_t *instance);

#endif
