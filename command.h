#ifndef REDROID_COMMAND_HDR
#define REDROID_COMMAND_HDR
#include "irc.h"
#include <stdbool.h>

typedef struct cmd_link_s    cmd_link_t;
typedef struct cmd_channel_s cmd_channel_t;
typedef struct cmd_entry_s   cmd_entry_t;

cmd_entry_t *cmd_entry_create(
    cmd_channel_t *associated,
    irc_t         *irc,
    const char    *channel,
    const char    *user,
    const char    *message,
    void         (*method)(irc_t *, const char *, const char *, const char *)
);

void cmd_entry_destroy(cmd_entry_t *entry);
void cmd_channel_rdclose(cmd_channel_t *channel);
void cmd_channel_wrclose(cmd_channel_t *channel);

bool cmd_channel_push(cmd_channel_t *channel, cmd_entry_t *entry);
bool cmd_channel_begin(cmd_channel_t *channel);
bool cmd_channel_ready(cmd_channel_t *channel);
bool cmd_channel_timeout(cmd_channel_t *channel);
void cmd_channel_process(cmd_channel_t *channel);
cmd_channel_t *cmd_channel_create(void);
void cmd_channel_destroy(cmd_channel_t *channel);

#endif
