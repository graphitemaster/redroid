#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(family);

static bool family_split(const char *string, string_t **nick, string_t **message) {
    if (!string)
        return false;

    char *space = strchr(string, ' ');
    if (!space)
        return false;

    *nick = string_construct();
    for (const char *from = string; from != space; from++)
        string_catf(*nick, "%c", *from);

    space++;
    while (isspace(*space))
        space++;

    *message = string_create(space);

    return true;
}

static int family_length(void) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM FAMILY");
    if (!statement)
        return 0;

    database_row_t *row = database_row_extract(statement, "i");
    int count = database_row_pop_integer(row);

    if (!database_statement_complete(statement))
        return 0;

    return count;
}

// family -help
static void family_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: family [<target> [nick]|<-stats|-add|-replace|-concat|-forget> <nick> <status>]", user);
    irc_write(irc, channel, "%s: used -> (target) is the <status> in our screwed up family", user);
}

// family <nick>
static const char *family_get(const char *nick) {
    database_statement_t *statement = database_statement_create("SELECT CONTENT FROM FAMILY WHERE NAME=?");

    if (!statement || !database_statement_bind(statement, "s", nick))
        return NULL;

    database_row_t *row = database_row_extract(statement, "s");

    if (!row)
        return NULL;

    const char *status = database_row_pop_string(row);

    if (!database_statement_complete(statement))
        return NULL;

    return status;
}

static void family_entry_random(irc_t *irc, const char *channel, const char *user) {
    database_statement_t *statement = database_statement_create("SELECT NAME, CONTENT FROM FAMILY ORDER BY RANDOM() LIMIT 1");
    if (!statement)
        return;

    database_row_t *row = database_row_extract(statement, "ss");
    if (!row)
        return;

    const char *name    = database_row_pop_string(row);
    const char *content = database_row_pop_string(row);

    database_request(irc, "FAMILY");
    irc_write(irc, channel, "%s: %s is the %s in our screwed up family", user, name, content);
}

static void family_entry(irc_t *irc, const char *channel, const char *user, const char *message) {
    const char *get = family_get(message);

    if (!get) {
        irc_write(irc, channel, "%s: Sorry, \"%s\" is not in my Family list.", user, message);
        return;
    }

    database_request(irc, "FAMILY");

    irc_write(irc, channel, "%s: %s is the %s in our screwed up family", user, message, get);
}

static void family_stats(irc_t *irc, const char *channel, const char *user) {
    int count   = family_length();
    int request = database_request_count(irc, "FAMILY");

    irc_write(irc, channel, "%s: family stats -> %d members -> requested %d times", user, count, request);
}

static void family_add_replace(irc_t *irc, const char *channel, const char *user, const char *message, bool replace) {
    string_t *member = NULL;
    string_t *status = NULL;

    if (!family_split(message, &member, &status))
        return family_help(irc, channel, user);

    const char *exists = family_get(string_contents(member));
    if (exists && !replace) {
        irc_write(
            irc,
            channel,
            "%s: Sorry, but \"%s\" is already: \"%s\" - please use -replace or -concat to modify it",
            user,
            string_contents(member),
            exists
        );
        return;
    }

    database_statement_t *statement = (replace) ? database_statement_create("UPDATE FAMILY SET CONTENT=? WHERE NAME=?")
                                                : database_statement_create("INSERT INTO FAMILY (NAME, CONTENT) VALUES ( ?, ? )");
    if (!statement)
        return;

    if (replace) {
        if (!database_statement_bind(statement, "SS", status, member))
            return;
    } else {
        if (!database_statement_bind(statement, "SS", member, status))
            return;
    }

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, %s family member: %s is the \"%s\"",
        user,
        (replace) ? "replaced" : "added",
        string_contents(member),
        string_contents(status)
    );
}

static void family_concat(irc_t *irc, const char *channel, const char *user, const char *message) {
    string_t *member = NULL;
    string_t *status = NULL;

    if (!family_split(message, &member, &status))
        return family_help(irc, channel, user);

    const char *get = family_get(string_contents(member));
    if (!get) {
        irc_write(irc, channel, "Sorry, couldn't find family member \"%s\"", string_contents(member));
        return;
    }

    database_statement_t *statement = database_statement_create("UPDATE FAMILY SET CONTENT=? WHERE NAME=?");
    string_t             *content   = string_create(get);

    string_catf(content, " %s", string_contents(status));

    if (!database_statement_bind(statement, "SS", member, content))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, family member: %s is the \"%s\"",
        user,
        string_contents(member),
        string_contents(content)
    );

}

static void family_forget(irc_t *irc, const char *channel, const char *user, const char *message) {
    char *strip = strchr(message, ' ');
    if (strip)
        *strip = '\0';

    if (!family_get(message)) {
        irc_write(irc, user, "Sorry, couldn't find family member \"%s\"", message);
        return;
    }

    database_statement_t *statement = database_statement_create("DELETE FROM FAMILY WHERE NAME=?");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "s", message))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed - family member \"%s\" from family", user, message);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return family_entry_random(irc, channel, user);

    list_t     *list   = strnsplit(strdup(message), " ", 2);
    const char *method = list_shift(list);

    if (!strcmp(method, "-help"))
        return family_help(irc, channel, user);
    if (!strcmp(method, "-add"))
        return family_add_replace(irc, channel, user, list_shift(list), false);
    if (!strcmp(method, "-replace"))
        return family_add_replace(irc, channel, user, list_shift(list), true);
    if (!strcmp(method, "-concat"))
        return family_concat(irc, channel, user, list_shift(list));
    if (!strcmp(method, "-forget"))
        return family_forget(irc, channel, user, list_shift(list));
    if (!strcmp(method, "-stats"))
        return family_stats(irc, channel, user);
    family_entry(irc, channel, user, message);
}
