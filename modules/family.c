#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(family);

static bool family_split(module_t *module, const char *string, string_t **nick, string_t **message) {
    if (!string)
        return false;

    char *space = strchr(string, ' ');
    if (!space)
        return false;

    *nick = string_construct(module);
    for (const char *from = string; from != space; from++)
        string_catf(*nick, "%c", *from);

    *message = string_create(module, &space[1]);
    return true;
}

static int family_length(module_t *module) {
    database_statement_t *statement = database_statement_create(module, "SELECT COUNT(*) FROM FAMILY");
    if (!statement)
        return 0;

    database_row_t *row = database_row_extract(module, statement, "i");
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
static const char *family_get(module_t *module, const char *nick) {
    database_statement_t *statement = database_statement_create(module, "SELECT CONTENT FROM FAMILY WHERE NAME=?");

    if (!statement || !database_statement_bind(statement, "s", nick))
        return NULL;

    database_row_t *row = database_row_extract(module, statement, "s");

    if (!row)
        return NULL;

    const char *status = database_row_pop_string(module, row);

    if (!database_statement_complete(statement))
        return NULL;

    return status;
}

static void family_entry(module_t *module, const char *channel, const char *user, const char *message) {
    const char *get = family_get(module, message);

    if (!get) {
        irc_write(module->instance, channel, "%s: Sorry, \"%s\" is not in my Family list.", user, message);
        return;
    }

    database_request(module->instance->database, "FAMILY");

    irc_write(module->instance, channel, "%s: %s is the %s in our screwed up family", user, message, get);
}

static void family_stats(module_t *module, const char *channel, const char *user) {
    int count   = family_length(module);
    int request = database_request_count(module->instance->database, "FAMILY");

    irc_write(module->instance, channel, "%s: family stats -> %d members -> requested %d times", user, count, request);
}

static void family_add(module_t *module, const char *channel, const char *user, const char *message) {
    string_t *member = NULL;
    string_t *status = NULL;

    if (!family_split(module, message, &member, &status))
        return family_help(module->instance, channel, user);

    const char *exists = family_get(module, string_contents(member));
    if (exists) {
        irc_write(
            module->instance,
            channel,
            "%s: Sorry, but \"%s\" is already: \"%s\" - please use -replace or -concat to modify it",
            user,
            string_contents(member),
            exists
        );
        return;
    }

    database_statement_t *statement = database_statement_create(module, "INSERT INTO FAMILY (NAME, CONTENT) VALUES ( ?, ? )");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "ss", string_contents(member), string_contents(status)))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(module->instance, channel, "%s: Ok, added family member: %s is the \"%s\"",
        user,
        string_contents(member),
        string_contents(status)
    );
}

static void family_forget(module_t *module, const char *channel, const char *user, const char *message) {
    char *strip = strchr(message, ' ');
    if (strip)
        *strip = '\0';

    if (!family_get(module, message)) {
        irc_write(module->instance, user, "Sorry, couldn't find family member \"%s\"", message);
        return;
    }

    database_statement_t *statement = database_statement_create(module, "DELETE FROM FAMILY WHERE NAME=?");
    if (!statement)
        return;

    if (!database_statement_bind(statement, "s", message))
        return;

    if (!database_statement_complete(statement))
        return;

    irc_write(module->instance, channel, "%s: Ok, removed - family member \"%s\" from family", user, message);
}

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    if (strstr(message, "-help") == &message[0])
        return family_help(module->instance, channel, user);
    else if (strstr(message, "-add") == &message[0])
        return family_add(module, channel, user, &message[5]);
    else if (strstr(message, "-forget") == &message[0])
        return family_forget(module, channel, user, &message[8]);
    else if (strstr(message, "-stats") == &message[0])
        return family_stats(module, channel, user);
    return family_entry(module, channel, user, message);
}

void module_close(module_t *module) {
}
