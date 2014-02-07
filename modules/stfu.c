#include <module.h>
#include <string.h>
#include <strings.h>

MODULE_DEFAULT(stfu);

static bool stfu_upper(char *input) {
    while (*input) {
        if ((*input >= 'a') && (*input <= 'z')) *input -= 'a'-'A';
        input++;
    }
    return true;
}

static bool stfu_type(char *input) {
    if (!strcasecmp(input, "INSERT"))  return stfu_upper(input);
    if (!strcasecmp(input, "OBJECT"))  return stfu_upper(input);
    if (!strcasecmp(input, "ORIFICE")) return stfu_upper(input);
    return false;
}

static void stfu_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: stfu [<nick>|<-stats>|<-add|-forget|-search> <insert|object|orifice> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> (attacker) <insert> <object> into (victims)'s <orifice> and tells them to STFU", user);
}

static bool stfu_select_find(const char *thing, const char *content) {
    string_t *string = string_format("SELECT COUNT(*) FROM %s WHERE CONTENT=?", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    if (!database_statement_bind(statement, "s", content))
        return false;

    database_row_t *grab = database_row_extract(statement, "i");
    if (database_row_pop_integer(grab) >= 1)
        return true;

    return false;
}

static void stfu_add(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *thing   = list_shift(list);
    char *content = list_shift(list);

    if (!stfu_type(thing))
        return stfu_help(irc, channel, user);

    if (stfu_select_find(thing, content)) {
        irc_write(irc, channel, "%s: %s %s already exists", user, thing, content);
        return;
    }

    string_t *string = string_format("INSERT INTO %s (CONTENT) VALUES ( ? )", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    if (!database_statement_bind(statement, "s", content))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, added %s to %s list", user, content, thing);
}

static void stfu_forget(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *thing   = list_shift(list);
    char *content = list_shift(list);

    if (!stfu_type(thing))
        return stfu_help(irc, channel, user);

    if (!stfu_select_find(thing, content)) {
        irc_write(irc, channel, "%s: %s %s doesn't exist", user, thing, content);
        return;
    }

    string_t *string = string_format("DELETE FROM %s WHERE CONTENT=?", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    if (!database_statement_bind(statement, "s", content))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed %s from %s list", user, content, thing);
}

static int stfu_count(const char *thing) {
    int                   count     = -1;
    string_t             *string    = string_format("SELECT COUNT(*) FROM %s", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    database_row_t       *row       = database_row_extract(statement, "i");

    if (!row)
        return -1;

    count = database_row_pop_integer(row);

    if (!database_statement_complete(statement))
        return -1;

    return count;
}

static void stfu_stats(irc_t *irc, const char *channel, const char *user) {
    int stfus    = database_request_count(irc, "STFU");
    int inserts  = stfu_count("INSERT");
    int objects  = stfu_count("OBJECT");
    int orifices = stfu_count("ORIFICE");

    irc_write(irc, channel, "%s: stfu stats -> %d inserts; %d objects; %d orifices; -> shut %d people up",
        user, inserts, objects, orifices, stfus);
}

static void stfu_random(irc_t *irc, const char *channel, const char *user, const char *victim) {
    database_statement_t *insert   = database_statement_create("SELECT * FROM INSERT ORDER BY RANDOM() LIMIT 1");
    database_statement_t *object   = database_statement_create("SELECT * FROM OBJECT ORDER BY RANDOM() LIMIT 1");
    database_statement_t *orifice  = database_statement_create("SELECT * FROM ORIFICE ORDER BY RANDOM() LIMIT 1");
    database_row_t       *irow     = database_row_extract(insert,  "s");
    database_row_t       *orow     = database_row_extract(object,  "s");
    database_row_t       *rrow     = database_row_extract(orifice, "s");
    const char           *minsert  = database_row_pop_string(irow);
    const char           *mobject  = database_row_pop_string(orow);
    const char           *morifice = database_row_pop_string(rrow);

    if (!minsert || !mobject || !morifice)
        return;

    // TODO: when no victim, or victim is the user then randomly select
    // someone from the channel as the attacker.

    irc_write(irc, channel,
        "%s %s %s into %s's %s and tells them to shut the fuck up",
        user, minsert, mobject, victim, morifice);

    database_statement_complete(insert);
    database_statement_complete(object);
    database_statement_complete(orifice);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t *list   = strnsplit(message, " ", 2);
    char   *method = list_shift(list);

    if (!method || !strcmp(method, "-help"))
        return stfu_help(irc, channel, user);

    if (!strcmp(method, "-add"))    return stfu_add(irc, channel, user, list);
    if (!strcmp(method, "-forget")) return stfu_forget(irc, channel, user, list);
    if (!strcmp(method, "-stats"))  return stfu_stats(irc, channel, user);

    stfu_random(irc, channel, user, message);
}
