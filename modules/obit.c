#include <module.h>
#include <string.h>

MODULE_DEFAULT(obit);

static bool obit_upper(char *input) {
    while (*input) {
        if ((*input >= 'a') && (*input <= 'z')) *input -= 'a'-'A';
        input++;
    }
    return true;
}

static bool obit_type(char *input) {
    if (!strcasecmp(input, "KILLED"))  return obit_upper(input);
    if (!strcasecmp(input, "OBJECT"))  return obit_upper(input);
    if (!strcasecmp(input, "SUICIDE")) return obit_upper(input);
    if (!strcasecmp(input, "STYLE"))   return obit_upper(input);
    return false;
}

static void obit_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: obit [<nick>|<-stats>|<-add|-forget|-search> <killed|object|suicide|style> <phrase>]", user);
    irc_write(irc, channel, "%s: used -> (victim) was <killed> with <object> [by (attacker) [style]|<suicide>]", user);
}

static bool obit_select_find(const char *thing, const char *content) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM ? WHERE CONTENT=?");
    if (!database_statement_bind(statement, "ss", thing, content))
        return false;

    database_row_t *grab = database_row_extract(statement, "i");
    if (database_row_pop_integer(grab) >= 1)
        return true;

    return false;
}

static void obit_add(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *thing   = list_shift(list);
    char *content = list_shift(list);

    if (!obit_type(thing))
        return obit_help(irc, channel, user);

    if (obit_select_find(thing, content)) {
        irc_write(irc, channel, "%s: %s %s already exists", user, thing, content);
        return;
    }

    string_t *string = string_format("INSERT INTO %s (CONTENT) VALUES ( ? )", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    if (!database_statement_bind(statement, "s", content))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, added %s %s", user, thing, content);
}

static void obit_forget(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *thing   = list_shift(list);
    char *content = list_shift(list);

    if (!obit_type(thing))
        return obit_help(irc, channel, user);

    if (!obit_select_find(thing, content)) {
        irc_write(irc, channel, "%s: %s %s doesn't exist", user, thing, content);
        return;
    }

    string_t *string = string_format("DELETE * FROM %s WHERE CONTENT=?", thing);
    database_statement_t *statement = database_statement_create(string_contents(string));
    if (!database_statement_bind(statement, "s", content))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed %s %s", user, thing, content);
}

static int obit_count(const char *thing) {
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

static void obit_stats(irc_t *irc, const char *channel, const char *user) {
    int frags    = database_request_count(irc, "OBIT");
    int killed   = obit_count("KILLED");
    int objects  = obit_count("OBJECT");
    int suicides = obit_count("SUICIDE");
    int styles   = obit_count("STYLE");

    irc_write(irc, channel, "%s: obit stats -> %d killed; %d objects; %d suicides; %d styles -> announced %d frags",
        user, killed, objects, suicides, styles, frags);
}

static void obit_random(irc_t *irc, const char *channel, const char *user, list_t *list) {
    database_statement_t *killed  = database_statement_create("SELECT * FROM KILLED ORDER BY RANDOM() LIMIT 1");
    database_statement_t *object  = database_statement_create("SELECT * FROM OBJECT ORDER BY RANDOM() LIMIT 1");
    database_row_t       *krow    = database_row_extract(killed, "s");
    database_row_t       *orow    = database_row_extract(object, "s");
    const char           *mkilled = database_row_pop_string(krow);
    const char           *mobject = database_row_pop_string(orow);

    if (!mkilled || !mobject)
        return;

    const char *victim = list_shift(list);
    if (!strcmp(victim, user)) {
        database_statement_t *suicide  = database_statement_create("SELECT * FROM SUICIDE ORDER BY RANDOM() LIMIT 1");
        database_row_t       *row      = database_row_extract(suicide, "s");
        const char           *msuicide = database_row_pop_string(row);

        if (!msuicide || !database_statement_complete(suicide))
            return;

        irc_write(irc, channel, "%s: %s was %s with %s %s", user, user, mkilled, mobject, msuicide);
        database_request(irc, "OBIT");
    } else {
        database_statement_t *style   = database_statement_create("SELECT * FROM STYLE ORDER BY RANDOM() LIMIT 1");
        database_row_t       *row     = database_row_extract(style, "s");
        const char           *mstyle  = database_row_pop_string(row);

        if (!mstyle || !database_statement_complete(style))
            return;

        irc_write(irc, channel, "%s: %s was %s with %s by %s %s", user, victim, mkilled, mobject, user, mstyle);
        database_request(irc, "OBIT");
    }

    database_statement_complete(killed);
    database_statement_complete(object);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t *list   = strnsplit(message, " ", 2);
    char   *method = list_shift(list);

    if (!method || !strcmp(method, "-help"))
        return obit_help(irc, channel, user);

    if (!strcmp(method, "-add"))    return obit_add(irc, channel, user, list);
    if (!strcmp(method, "-forget")) return obit_forget(irc, channel, user, list);
    if (!strcmp(method, "-stats"))  return obit_stats(irc, channel, user);

    /* Put the method back into the list and do me some random */
    list_push(list, method);
    obit_random(irc, channel, user, list);
}
