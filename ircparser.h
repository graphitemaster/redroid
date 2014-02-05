#ifndef REDROID_IRCPARSER_HDR
#define REDROID_IRCPARSER_HDR
#include <stddef.h>
#include <stdbool.h>

typedef struct irc_s irc_t;

typedef enum {
    IRC_PARSER_STATE_INIT,
    IRC_PARSER_STATE_NICK,
    IRC_PARSER_STATE_NAME,
    IRC_PARSER_STATE_HOST,
    IRC_PARSER_STATE_COMMAND,
    IRC_PARSER_STATE_PARMETERS,
    IRC_PARSER_STATE_TRAILING,
    IRC_PARSER_STATE_END,
    IRC_PARSER_STATE_ERROR
} irc_parser_state_t;

typedef enum {
    IRC_PARSER_ERROR_NONE,
    IRC_PARSER_ERROR_PARSE,
    IRC_PARSER_ERROR_STATE,
    IRC_PARSER_ERROR_LENGTH
} irc_parser_error_t;

typedef struct {
    char  *content;
    size_t length;
} irc_parser_data_t;

typedef struct {
    size_t             length;
    size_t             last;
    char               buffer[513];
    irc_parser_state_t state;
    irc_parser_error_t error;
    irc_t             *instance;
    void             (*callbacks[IRC_PARSER_STATE_ERROR+1])(irc_t *, irc_parser_data_t *);
} irc_parser_t;

int irc_parser_next(irc_parser_t *parser, const char *data, size_t length);
void irc_parser_init(irc_parser_t *parser, irc_t *irc);

#endif
