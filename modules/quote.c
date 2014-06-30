#include <module.h>
#include <string.h>

#define ACCESS 4

MODULE_DEFAULT(quote);

static bool quote_access_check(irc_t *irc, const char *user) {
    if (access_range(irc, user, ACCESS))
        return true;

    int access = 0;
    access_level(irc, user, &access);
    irc_write(irc, user, "Sorry, you have level %d but need level %d to do that", access, ACCESS);
    return false;
}

static void quote_nick_stripspecial(char **input) {
    char *beg = *input;
    char *end = &beg[strlen(beg)-1];
    if (*beg == '<') beg++;
    if (*beg == '@') beg++;
    if (*end == '>' || *end == ':') end--;
    *++end ='\0';
    *input = beg;
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
    if (!database_statement_bind(statement, "s", user))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if ((*count = database_row_pop_integer(row)) >= 1)
        return true;

    return false;
}

static bool quote_find(const char *user, const char *quote) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM QUOTES WHERE NAME=? AND CONTENT=?");
    if (!database_statement_bind(statement, "ss", user, quote))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if (database_row_pop_integer(row) >= 1)
        return true;

    return false;
}

/* quote -help */
static void quote_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: quote [<nick> [searchtext]|<-stats [nick]|-add|-forget|-reauthor <oldnick> <newnick>> <nickname> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> <<nickname>> <phrase>", user);
}

/* quote */
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

/* quote <nick> */
static void quote_entry(irc_t *irc, const char *channel, const char *user, const char *nick) {
    database_statement_t *statement = database_statement_create("SELECT * FROM QUOTES WHERE NAME = ? ORDER BY RANDOM() LIMIT 1");
    if (!database_statement_bind(statement, "s", nick))
        return;
    database_row_t *row = database_row_extract(statement, "ss");
    if (!row)
        return irc_write(irc, channel, "%s: Sorry, could not find any quotes like \"%s\"", user, nick);

    const char *quotenick    = database_row_pop_string(row);
    const char *quotemessage = database_row_pop_string(row);

    if (!database_statement_complete(statement))
        return;

    database_request(irc, "QUOTES");

    irc_write(irc, channel, "%s: <%s> %s", user, quotenick, quotemessage);
}

static void quote_stats(irc_t *irc, const char *channel, const char *user, list_t *list) {
    const char *who = list_shift(list);
    if (!who) {
        int count   = quote_length();
        int request = database_request_count(irc, "QUOTES");

        return irc_write(irc, channel, "%s: quote stats -> %d quotes -> requested %d times", user, count, request);
    }

    int count = 0;
    if (!quote_count(who, &count))
        return irc_write(irc, channel, "%s: %s has no quotes", user, who);

    const char *plural = (count > 1) ? "quotes" : "quote";
    if (strcmp(user, who))
        irc_write(irc, channel, "%s: %s has %d %s", user, who, count, plural);
    else
        irc_write(irc, channel, "%s: you have %d %s", user, count, plural);
}

static void quote_add(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *quotenick    = list_shift(list);
    char *quotemessage = list_shift(list);

    if (!quotenick || !quotemessage)
        return;
    if (!quote_access_check(irc, user))
        return;

    quote_nick_stripspecial(&quotenick);

    if (quote_find(quotenick, quotemessage))
        return irc_write(irc, channel, "%s: Quote already exists.", user);

    database_statement_t *statement = database_statement_create("INSERT INTO QUOTES (NAME, CONTENT) VALUES ( ?, ? )");
    if (!database_statement_bind(statement, "ss", quotenick, quotemessage))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, added quote: <%s> %s", user, quotenick, quotemessage);
}

static void quote_forget(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *quotenick    = list_shift(list);
    char *quotemessage = list_shift(list);

    if (!quotenick || !quotemessage)
        return;
    if (!quote_access_check(irc, user))
        return;

    quote_nick_stripspecial(&quotenick);

    if (!quote_find(quotenick, quotemessage))
        return irc_write(irc, channel, "%s: Sorry, could not find any quotes like \"%s %s\"",
            user,
            quotenick,
            quotemessage
        );

    database_statement_t *statement = database_statement_create("DELETE FROM QUOTES WHERE NAME = ? AND CONTENT = ?");
    if (!database_statement_bind(statement, "ss", quotenick, quotemessage))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed - \"%s %s..\" - from the quote db",
        user,
        quotenick,
        quotemessage
    );
}

static void quote_reauthor(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *from = list_shift(list);
    char *to   = list_shift(list);

    if (!from || !to)
        return quote_help(irc, channel, user);
    if (!quote_access_check(irc, user))
        return;

    quote_nick_stripspecial(&from);
    quote_nick_stripspecial(&to);

    int count = 0;
    if (!quote_count(from, &count))
        return irc_write(irc, channel, "%s: Sorry, could not find any quotes by \"%s\"", user, from);

    database_statement_t *statement = database_statement_create("UPDATE QUOTES SET NAME=? WHERE NAME=?");
    if (!database_statement_bind(statement, "ss", to, from))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, reauthored %d %s from %s to %s", user, count, count > 1 ? "quotes" : "quote", from, to);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return quote_entry_random(irc, channel, user);

    list_t     *list   = strnsplit(message, " ", 2);
    const char *method = list_shift(list);

    if (!strcmp(method, "-help"))     return quote_help(irc, channel, user);
    if (!strcmp(method, "-add"))      return quote_add(irc, channel, user, list);
    if (!strcmp(method, "-forget"))   return quote_forget(irc, channel, user, list);
    if (!strcmp(method, "-stats"))    return quote_stats(irc, channel, user, list);
    if (!strcmp(method, "-reauthor")) return quote_reauthor(irc, channel, user, list);

    return quote_entry(irc, channel, user, method);
}
