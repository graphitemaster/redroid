#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(quote);

// TODO make better
static bool quote_split(const char *string, string_t **nick, string_t **message) {
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
    *nick = string_construct();
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
    *message = string_create(string);
    return true;
}

static int quote_length(void) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM QUOTES");
    if (!statement)
        return 0;

    database_row_t *row = database_row_extract(statement, "i");
    int count = database_row_pop_integer(row);

    if (!database_statement_complete(statement))
        return 0;

    return count;
}

static bool quote_count(const char *user, int *count) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM QUOTES WHERE NAME=?");
    if (!statement || !database_statement_bind(statement, "s", user))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if ((*count = database_row_pop_integer(row)) >= 1)
        return true;

    return false;
}

static bool quote_find(const char *user, const char *quote) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM QUOTES WHERE NAME=? AND CONTENT=?");
    if (!statement || !database_statement_bind(statement, "ss", user, quote))
        return false;

    database_row_t *row = database_row_extract(statement, "i");

    if (database_row_pop_integer(row) >= 1)
        return true;

    return false;
}

// quote -help
static void quote_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: quote [<nick> [searchtext]|<-stats [nick]|-add|-forget|-reauthor <oldnick> <newnick>> <nickname> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> <<nickname>> <phrase>", user);
}

// quote
static void quote_entry_random(irc_t *irc, const char *channel, const char *user) {
    database_statement_t *statement = database_statement_create("SELECT * FROM QUOTES ORDER BY RANDOM() LIMIT 1");
    if (!statement)
        return;

    database_row_t *row = database_row_extract(statement, "ss");

    if (!row)
        return;

    const char *nick    = database_row_pop_string(row);
    const char *message = database_row_pop_string(row);

    if (!database_statement_complete(statement))
        return;

    database_request(irc, "QUOTES");

    irc_write(irc, channel, "%s: <%s> %s", user, nick, message);
}

// quote <nick>
static void quote_entry(irc_t *irc, const char *channel, const char *user, const char *message) {
    database_statement_t *statement = database_statement_create("SELECT * FROM QUOTES WHERE NAME = ? ORDER BY RANDOM() LIMIT 1");

    if (!statement || !database_statement_bind(statement, "s", message))
        return;

    database_row_t *row = database_row_extract(statement, "ss");

    if (!row) {
        irc_write(irc, channel, "%s: Sorry, could not find any quotes like \"%s\"", user, message);
        return;
    }

    const char *quotenick    = database_row_pop_string(row);
    const char *quotemessage = database_row_pop_string(row);

    if (!database_statement_complete(statement))
        return;

    database_request(irc, "QUOTES");

    irc_write(irc, channel, "%s: <%s> %s", user, quotenick, quotemessage);
}

static void quote_stats(irc_t *irc, const char *channel, const char *user, const char *who) {
    if (!who) {
        int count   = quote_length();
        int request = database_request_count(irc, "QUOTES");

        irc_write(irc, channel, "%s: quote stats -> %d quotes -> requested %d times", user, count, request);
    }

    int count = 0;
    if (!quote_count(who, &count))
        return;

    irc_write(irc, channel, "%s: %s has %d quotes", user, who, count);
}

static void quote_add(irc_t *irc, const char *channel, const char *user, const char *message) {
    string_t *quotenick    = NULL;
    string_t *quotemessage = NULL;

    if (!quote_split(message, &quotenick, &quotemessage))
        return;

    if (quote_find(string_contents(quotenick), string_contents(quotemessage))) {
        irc_write(irc, channel, "%s: Quote already exists.", user);
        return;
    }

    database_statement_t *statement = database_statement_create("INSERT INTO QUOTES (NAME, CONTENT) VALUES ( ?, ? )");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "SS", quotenick, quotemessage))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, added quote: <%s> %s",
        user,
        string_contents(quotenick),
        string_contents(quotemessage)
    );
}

static void quote_forget(irc_t *irc, const char *channel, const char *user, const char *message) {
    string_t *quotenick    = NULL;
    string_t *quotemessage = NULL;

    if (!quote_split(message, &quotenick, &quotemessage))
        return;

    if (!quote_find(string_contents(quotenick), string_contents(quotemessage))) {
        irc_write(irc, channel, "%s: Sorry, could not find any quotes like \"%s %s\"",
            user,
            string_contents(quotenick),
            string_contents(quotemessage)
        );
        return;
    }

    database_statement_t *statement = database_statement_create("DELETE FROM QUOTES WHERE NAME = ? AND CONTENT = ?");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "ss", string_contents(quotenick), string_contents(quotemessage)))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed - \"%s %s..\" - from the quote db",
        user,
        string_contents(quotenick),
        string_contents(quotemessage)
    );
}

static void quote_reauthor(irc_t *irc, const char *channel, const char *user, const char *who) {
    int count = 0;

    if (!strchr(who, ' '))
        return quote_help(irc, channel, user);

    char *from = strdup(who);
    char *to   = strchr(who, ' ');

    if (!to)
        return quote_help(irc, channel, user);

    to++;
    *strchr(from, ' ')='\0';

    if (!quote_count(from, &count)) {
        irc_write(irc, channel, "%s: Sorry, could not find any quotes by \"%s\"", user, from);
        return;
    }

    // reauthor
    database_statement_t *statement = database_statement_create("UPDATE QUOTES SET NAME=? WHERE NAME=?");

    if (!statement || !database_statement_bind(statement, "ss", to, from))
        return;

    if (!database_statement_complete(statement))
        return;

    if (count == 1)
        irc_write(irc, channel, "%s: Ok, reauthored 1 quote from %s to %s", user, from, to);
    else
        irc_write(irc, channel, "%s: Ok, reauthored %d quotes from %s to %s", user, count, from, to);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return quote_entry_random(irc, channel, user);
    else if (strstr(message, "-help") == &message[0])
        return quote_help(irc, channel, user);
    else if (strstr(message, "-add") == &message[0])
        return quote_add(irc, channel, user, &message[5]);
    else if (strstr(message, "-forget") == &message[0])
        return quote_forget(irc, channel, user, &message[8]);
    else if (strstr(message, "-stats") == &message[0])
        return quote_stats(irc, channel, user, &message[7]);
    else if (strstr(message, "-reauthor") == &message[0])
        return quote_reauthor(irc, channel, user, &message[10]);
    return quote_entry(irc, channel, user, message);
}
