#include "irc.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    IRC_PARSER_CONSUME_ERROR,
    IRC_PARSER_CONSUME_STOP,
    IRC_PARSER_CONSUME_UNDO,
    IRC_PARSER_CONSUME_CONTINUE
} irc_consume_t;

static const size_t irc_parser_state_next[] = {
    [IRC_PARSER_STATE_INIT]      = IRC_PARSER_STATE_NICK,
    [IRC_PARSER_STATE_NICK]      = IRC_PARSER_STATE_NAME,
    [IRC_PARSER_STATE_NAME]      = IRC_PARSER_STATE_HOST,
    [IRC_PARSER_STATE_HOST]      = IRC_PARSER_STATE_COMMAND,
    [IRC_PARSER_STATE_COMMAND]   = IRC_PARSER_STATE_PARMETERS,
    [IRC_PARSER_STATE_PARMETERS] = IRC_PARSER_STATE_TRAILING,
    [IRC_PARSER_STATE_TRAILING]  = IRC_PARSER_STATE_END,
    [IRC_PARSER_STATE_END]       = IRC_PARSER_STATE_INIT,
    [IRC_PARSER_STATE_ERROR]     = IRC_PARSER_STATE_ERROR
};

static void irc_parser_progress(irc_parser_t *parser) {
    parser->state = irc_parser_state_next[parser->state];
    parser->last  = parser->length;
}

static void irc_parser_call(irc_parser_t *parser, irc_parser_state_t state) {
    if (!parser->callbacks[state])
        return;

    size_t length = parser->length - parser->last - 1;
    parser->buffer[parser->last + length] = 0;
    parser->callbacks[state] (
        parser->instance,
        &(irc_parser_data_t) {
            .content = parser->buffer + parser->last,
            .length  = length
        }
    );

}

static void irc_parser_dispatch(irc_parser_t *parser) {
    irc_parser_call(parser, parser->state);
}

static void irc_parser_instrument(irc_parser_t *parser) {
    irc_parser_dispatch(parser);
    irc_parser_progress(parser);
}

static void irc_parser_append(irc_parser_t *parser, char ch) {
    parser->buffer[parser->length++] = ch;
    if (parser->length > 512) {
        parser->state  = IRC_PARSER_STATE_ERROR;
        parser->error  = IRC_PARSER_ERROR_LENGTH;
    }
}

static void irc_parser_error(irc_parser_t *parser, irc_parser_error_t error) {
    parser->error = error;
    parser->state = IRC_PARSER_STATE_ERROR;
}

static irc_consume_t irc_parser_interpret(irc_parser_t *parser, const char ch) {
    switch (parser->state) {
        case IRC_PARSER_STATE_INIT:
            if (ch == ':') {
                parser->last  = 1;
                parser->state = IRC_PARSER_STATE_NICK;
            } else {
                parser->length--;
                parser->state = IRC_PARSER_STATE_COMMAND;
                return IRC_PARSER_CONSUME_UNDO;
            }
            break;

        case IRC_PARSER_STATE_NICK:
            if (ch == '!')
                irc_parser_instrument(parser);
            break;
        case IRC_PARSER_STATE_NAME:
            if (ch == '@')
                irc_parser_instrument(parser);
            break;
        case IRC_PARSER_STATE_HOST:
            if (ch == ' ')
                irc_parser_instrument(parser);
            break;
        case IRC_PARSER_STATE_COMMAND:
            if (ch == ' ')
                irc_parser_instrument(parser);
            break;

        case IRC_PARSER_STATE_PARMETERS:
            if (ch == ' ') {
                irc_parser_dispatch(parser);
                parser->last = parser->length;
            } else if (ch == ':' && parser->length == (parser->last + 1))
                irc_parser_progress(parser);
            break;

        case IRC_PARSER_STATE_TRAILING:
            break;

        default:
            irc_parser_error(parser, IRC_PARSER_ERROR_STATE);
            return IRC_PARSER_CONSUME_STOP;
    }
    return IRC_PARSER_CONSUME_CONTINUE;
}

static void irc_parser_reset(irc_parser_t *parser) {
    parser->length    = 0;
    parser->last      = 0;
    parser->state     = IRC_PARSER_STATE_INIT;
    parser->buffer[0] = '\0';
}

static irc_consume_t irc_parser_consume(irc_parser_t *parser, const char ch) {
    switch (ch) {
        case '\r':
            parser->state = IRC_PARSER_STATE_END;
            break;

        case '\n':
            if (parser->state != IRC_PARSER_STATE_END)
                return IRC_PARSER_CONSUME_ERROR;
            parser->length++;
            irc_parser_call(parser, IRC_PARSER_STATE_END);
            irc_parser_reset(parser);
            break;

        default:
            irc_parser_append(parser, ch);
            return irc_parser_interpret(parser, ch);
    }
    return IRC_PARSER_CONSUME_CONTINUE;
}

int irc_parser_next(irc_parser_t *parser, const char *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        switch (irc_parser_consume(parser, data[i])) {
            case IRC_PARSER_CONSUME_ERROR:
                return -1;
            case IRC_PARSER_CONSUME_STOP:
                return (int)i;
            case IRC_PARSER_CONSUME_UNDO:
                --i;
                break;
            case IRC_PARSER_CONSUME_CONTINUE:
                break;
        }
    }
    return length;
}

void irc_parser_init(irc_parser_t *parser, irc_t *irc) {
    parser->instance = irc;
    irc_parser_reset(parser);
}
