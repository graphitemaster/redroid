#include <module.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(quote);

static sqlite3 *database = NULL;

// skip some whitespace
char *quote_skip(const char *input) {
    while (*input && isspace(*input))
        input++;
    return (char*)input;
}

// split a "USER MESSAGE" or "<USER> MESSAGE" into two strings
static bool quote_split(const char *input, string_t **nick, string_t **message) {
    char *copy;
    char *clean;

    if (!*input)
        return false;
    if (!*(input = quote_skip(input)))
        return false;

    clean = (*(copy = strdup(input)) == '<') ? quote_skip(&copy[1]) : copy;

    *nick = string_construct();
    for (const char *fill = clean; *fill && *fill != ((*copy == '<') ? '>' : ' '); fill++, clean++) {
        if (isspace(*fill)) continue;
        string_catf(*nick, "%c", *fill);
    }

    if (!*clean || *clean != ((*copy == '<') ? '>' : ' '))
        goto error;
    clean++;
    clean = quote_skip(clean);
    if (!*clean)
        goto error;

    *message = string_create(clean);
    free(copy);
    return true;

error:
    string_destroy(*nick);
    free(copy);
    return false;
}

static size_t quote_length(void) {
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM QUOTES", -1, &statement, NULL) != SQLITE_OK)
        return 0;

    if (sqlite3_step(statement) != SQLITE_ROW)
        goto error;

    size_t calculate = sqlite3_column_int(statement, 0);

    if (sqlite3_step(statement) != SQLITE_DONE)
        goto error;

    return calculate;
error:
    if (statement)
        sqlite3_finalize(statement);
    return 0;
}

static bool quote_find(const char *user, const char *quote) {
    const char   *statement_string = "SELECT * FROM QUOTES WHERE NAME = ? AND CONTENT = ?";
    sqlite3_stmt *statement        = NULL;

    if (sqlite3_prepare_v2(database, statement_string, -1, &statement, NULL) != SQLITE_OK)
        return false;

    if (sqlite3_bind_text(statement, 1, strdup(user), -1, &free) != SQLITE_OK)
        goto error;
    if (sqlite3_bind_text(statement, 2, strdup(quote), -1, &free) != SQLITE_OK)
        goto error;

    if (sqlite3_step(statement) != SQLITE_ROW)
        goto error;

    sqlite3_finalize(statement);
    return true;
error:
    sqlite3_finalize(statement);
    return false;
}

// quote -help
static void quote_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: quote [<nick> [searchtext]|<-stats>|<-add|-forget> <nickname> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> <<nickname>> <phrase>", user);
}

// quote -add
// quote -forget
static void quote_add_forget(irc_t *irc, const char *channel, const char *user, const char *message, bool forget) {
    string_t *quotenick;
    string_t *quotemessage;

    if (!quote_split(message, &quotenick, &quotemessage))
        return;

    const char   *statement_string;
    sqlite3_stmt *statement = NULL;

    if (forget) {
        if (!quote_find(string_contents(quotenick), string_contents(quotemessage))) {
            irc_write(irc, channel, "%s: Sorry, couldn't find \"%s %s\" in the quote db out of %zu entries",
                user,
                string_contents(quotenick),
                string_contents(quotemessage),
                quote_length()
            );
            string_destroy(quotenick);
            string_destroy(quotemessage);
            return;
        }
        statement_string = "DELETE FROM QUOTES WHERE NAME = ? AND CONTENT = ?";
    } else {
        statement_string = "INSERT INTO QUOTES (NAME, CONTENT) VALUES ( ?, ? )";
    }

    if (sqlite3_prepare_v2(database, statement_string, -1, &statement, NULL) != SQLITE_OK)
        goto process_error;

    // bind must copy out the strings
    if (sqlite3_bind_text(statement, 1, strdup(string_contents(quotenick)), -1, &free) != SQLITE_OK)
        goto process_error;
    if (sqlite3_bind_text(statement, 2, strdup(string_contents(quotemessage)), -1, &free) != SQLITE_OK)
        goto process_error;

    if (sqlite3_step(statement) != SQLITE_DONE)
        goto process_error;

    sqlite3_finalize(statement);

    if (forget) {
        irc_write(irc, channel, "%s: Ok, removed - \"%s %s..\" - from the quote db",
            user,
            string_contents(quotenick),
            string_contents(quotemessage)
        );
    } else {
        irc_write(irc, channel, "%s: Ok, added quote: <%s> %s",
            user,
            string_contents(quotenick),
            string_contents(quotemessage)
        );
    }

    string_destroy(quotenick);
    string_destroy(quotemessage);
    return;

process_error:
    if (statement)
        sqlite3_finalize(statement);

    string_destroy(quotenick);
    string_destroy(quotemessage);
}

// quote
static void quote_entry_random(irc_t *irc, const char *channel, const char *user) {
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database, "SELECT * FROM QUOTES ORDER BY RANDOM() LIMIT 1", -1, &statement, NULL) != SQLITE_OK)
        return;

    if (sqlite3_step(statement) != SQLITE_ROW)
        goto error;

    irc_write(irc, channel, "%s: <%s> %s", user,
        sqlite3_column_text(statement, 0),
        sqlite3_column_text(statement, 1)
    );

error:
    sqlite3_finalize(statement);
}

// quote <nick>
static void quote_entry(irc_t *irc, const char *channel, const char *user, const char *message) {
    const char   *statement_string = "SELECT * FROM QUOTES WHERE NAME = ? ORDER BY RANDOM() LIMIT 1";
    sqlite3_stmt *statement        = NULL;

    if (sqlite3_prepare_v2(database, statement_string, -1, &statement, NULL) != SQLITE_OK)
        goto entry_error;

    if (sqlite3_bind_text(statement, 1, strdup(message), -1, &free) != SQLITE_OK)
        goto entry_error;

    int step = sqlite3_step(statement);
    if (step != SQLITE_ROW && step != SQLITE_DONE)
        goto entry_error;

    if (step == SQLITE_DONE) {
        irc_write(irc, channel, "%s: Sorry, could not find any quotes like \"%s\"", user, message);
        goto entry_error;
    }

    irc_write(irc, channel, "%s: <%s> %s", user,
        sqlite3_column_text(statement, 0),
        sqlite3_column_text(statement, 1)
    );

    sqlite3_finalize(statement);
    return;

entry_error:
    if (statement)
        sqlite3_finalize(statement);
}

static void quote_stats(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: quote stats -> %zu quotes -> requested ? times", user, quote_length());
}

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    irc_t *irc   = module->instance;
    char  *error = NULL;
    if (!database) {
        if (sqlite3_open("quote.db", &database))
            goto quote_error;
        if (sqlite3_exec(database, "create table if not exists QUOTES (NAME, CONTENT)", NULL, NULL, &error) != SQLITE_OK)
            goto quote_error;
    }

    if (!message)
        return quote_entry_random(irc, channel, user);
    else if (!strcmp(message, "-help"))
        return quote_help(irc, channel, user);
    else if (strstr(message, "-add") == &message[0])
        return quote_add_forget(irc, channel, user, &message[5], false);
    else if (strstr(message, "-forget") == &message[0])
        return quote_add_forget(irc, channel, user, &message[8], true);
    else if (strstr(message, "-stats") == &message[0])
        return quote_stats(irc, channel, user);
    else
        return quote_entry(irc, channel, user, message);

    return;

quote_error:
    if (error) {
        irc_write(irc, channel, "%s: database failure: %s", user, error);
        sqlite3_free(error);
    } else {
        irc_write(irc, channel, "%s: database failure: %s", user, sqlite3_errmsg(database));
    }
    sqlite3_close(database);
    database = NULL;
}

void module_close(module_t *module) {
    if (database)
        sqlite3_close(database);
}
