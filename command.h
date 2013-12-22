#ifndef REDROID_COMMAND_HDR
#define REDROID_COMMAND_HDR
#include "irc.h"
#include <stdbool.h>

typedef struct cmd_pool_s cmd_pool_t;
typedef struct cmd_entry_s cmd_entry_t;

cmd_entry_t *cmd_entry_create (
    irc_t      *irc,
    const char *channel,
    const char *user,
    const char *message,
    void      (*entry)(irc_t *, const char *, const char *, const char *)
);


void cmd_entry_destroy(cmd_entry_t *entry);
cmd_pool_t *cmd_pool_create(void);
void cmd_pool_destroy(cmd_pool_t *pool);
void cmd_pool_queue(cmd_pool_t *pool, cmd_entry_t *entry);
void cmd_pool_begin(cmd_pool_t *pool);
bool cmd_pool_ready(cmd_pool_t *pool);
void cmd_pool_process(cmd_pool_t *pool);
#endif
