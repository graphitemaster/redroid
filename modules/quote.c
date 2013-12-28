#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(quote);

// TODO make better
static bool quote_split(module_t *module, const char *string, string_t **nick, string_t **message) {
    char *terminate = NULL;
    for (const char *find = ">| "; *find; find++) {
        if ((terminate = strchr(string, *find))) {
            if (*find == '|' && terminate[1] != ' ')
                continue;
            break;
        }
    }
    if (!terminate)
        return false;
    *nick = string_construct(module);
    for (; string != terminate; string++) {
        for (const char *strip = "< "; *strip; strip++)
            if (*string == *strip)
                goto next;
        string_catf(*nick, "%c", *string);
        next: ;
    }
    while (*string && (*string == '>' || *string == '|' || *string == ' '))
        string++;
    if (!*string)
        return false;
    *message = string_create(module, string);
    return true;
}

static int quote_length(module_t *module) {
    database_statement_t *statement = database_statement_create(module, "SELECT COUNT(*) FROM QUOTES");
    if (!statement)
        return 0;

    database_row_t *row = database_row_extract(module, statement, "i");
    int count = database_row_pop_integer(row);

    if (!database_statement_complete(statement))
        return 0;

    return count;
}

static bool quote_find(module_t *module, const char *user, const char *quote) {
    database_statement_t *statement = database_statement_create(module, "SELECT COUNT(*) FROM QUOTES WHERE NAME=? AND CONTENT=?");
    if (!statement || !database_statement_bind(statement, "ss", user, quote))
        return false;

    database_row_t *row = database_row_extract(module, statement, "i");

    if (database_row_pop_integer(row) >= 1)
        return true;

    return false;
}

// quote -help
static void quote_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: quote [<nick> [searchtext]|<-stats>|<-add|-forget> <nickname> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> <<nickname>> <phrase>", user);
}

// quote
static void quote_entry_random(module_t *module, const char *channel, const char *user) {
    database_statement_t *statement = database_statement_create(module, "SELECT * FROM QUOTES ORDER BY RANDOM() LIMIT 1");
    if (!statement)
        return;

    database_row_t *row = database_row_extract(module, statement, "ss");

    if (!row)
        return;

    const char *nick    = database_row_pop_string(module, row);
    const char *message = database_row_pop_string(module, row);

    if (!database_statement_complete(statement))
        return;

    database_request(module->instance->database, "QUOTES");

    irc_write(module->instance, channel, "%s: <%s> %s", user, nick, message);
}

// quote <nick>
static void quote_entry(module_t *module, const char *channel, const char *user, const char *message) {
    database_statement_t *statement = database_statement_create(module, "SELECT * FROM QUOTES WHERE NAME = ? ORDER BY RANDOM() LIMIT 1");

    if (!statement || !database_statement_bind(statement, "s", message))
        return;

    database_row_t *row = database_row_extract(module, statement, "ss");

    if (!row) {
        irc_write(module->instance, channel, "%s: Sorry, could not find any quotes like \"%s\"", user, message);
        return;
    }

    const char *quotenick    = database_row_pop_string(module, row);
    const char *quotemessage = database_row_pop_string(module, row);

    if (!database_statement_complete(statement))
        return;

    database_request(module->instance->database, "QUOTES");

    irc_write(module->instance, channel, "%s: <%s> %s", user, quotenick, quotemessage);
}

static void quote_stats(module_t *module, const char *channel, const char *user) {
    int count   = quote_length(module);
    int request = database_request_count(module->instance->database, "QUOTES");

    irc_write(module->instance, channel, "%s: quote stats -> %d quotes -> requested %d times", user, count, request);
}

static void quote_add(module_t *module, const char *channel, const char *user, const char *message) {
    string_t *quotenick    = NULL;
    string_t *quotemessage = NULL;

    if (!quote_split(module, message, &quotenick, &quotemessage))
        return;

    if (quote_find(module, string_contents(quotenick), string_contents(quotemessage))) {
        irc_write(module->instance, channel, "%s: Quote already exists.", user);
        return;
    }

    database_statement_t *statement = database_statement_create(module, "INSERT INTO QUOTES (NAME, CONTENT) VALUES ( ?, ? )");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "ss", string_contents(quotenick), string_contents(quotemessage)))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(module->instance, channel, "%s: Ok, added quote: <%s> %s",
        user,
        string_contents(quotenick),
        string_contents(quotemessage)
    );
}

static void quote_forget(module_t *module, const char *channel, const char *user, const char *message) {
    string_t *quotenick    = NULL;
    string_t *quotemessage = NULL;

    if (!quote_split(module, message, &quotenick, &quotemessage))
        return;

    if (!quote_find(module, string_contents(quotenick), string_contents(quotemessage))) {
        irc_write(module->instance, channel, "%s: Sorry, could not find any quotes like \"%s %s\"",
            user,
            string_contents(quotenick),
            string_contents(quotemessage)
        );
        return;
    }

    database_statement_t *statement = database_statement_create(module, "DELETE FROM QUOTES WHERE NAME = ? AND CONTENT = ?");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "ss", string_contents(quotenick), string_contents(quotemessage)))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(module->instance, channel, "%s: Ok, removed - \"%s %s..\" - from the quote db",
        user,
        string_contents(quotenick),
        string_contents(quotemessage)
    );
}

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    if (!message)
        return quote_entry_random(module, channel, user);
    else if (strstr(message, "-help") == &message[0])
        return quote_help(module->instance, channel, user);
    else if (strstr(message, "-add") == &message[0])
        return quote_add(module, channel, user, &message[5]);
    else if (strstr(message, "-forget") == &message[0])
        return quote_forget(module, channel, user, &message[8]);
    else if (strstr(message, "-stats") == &message[0])
        return quote_stats(module, channel, user);
    return quote_entry(module, channel, user, message);
}

void module_close(module_t *module) {
}
