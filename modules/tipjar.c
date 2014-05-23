#include <module.h>
#include <string.h>

MODULE_DEFAULT(tipjar);

#define TIPJAR_CENTS 5

static void tipjar_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: tipjar [-help|[<user> [-revert|<victim>]]] -> user must have level 4 or higher", user);
}

static bool tipjar_account_open(const char *target) {
    database_statement_t *stmt = database_statement_create("INSERT INTO TIPJAR VALUES(?, 0)");
    if (!database_statement_bind(stmt, "s", target) || !database_statement_complete(stmt))
        return false;
    return true;
}

static int tipjar_balance(const char *target) {
    database_statement_t *stmt = database_statement_create("SELECT CENTS FROM TIPJAR WHERE NAME = ?");
    if (!database_statement_bind(stmt, "s", target))
        return -1;
    database_row_t *row = database_row_extract(stmt, "i");
    if (!row)
        return -1;
    int value = database_row_pop_integer(row);
    if (!database_statement_complete(stmt))
        return -1;
    return value;
}

static void tipjar_status(irc_t *irc, const char *channel, const char *target) {
    int cents = tipjar_balance(target);
    if (cents == -1) {
        if (tipjar_account_open(target))
            return tipjar_status(irc, channel, target);
        return;
    }
    int  mdollars  = cents / 100;
    int  mcents    = cents % 100;
    bool neg       = false;

    if (mcents   < 0) { mcents   = -mcents;   neg = true; }
    if (mdollars < 0) { mdollars = -mdollars; neg = true; }


    irc_write(irc, channel,
        ((neg) ? "If %s had %d cents for every request, they would have: $-%d.%02d"
               : "If %s had %d cents for every request, they would have: $%d.%02d"), target, TIPJAR_CENTS, mdollars, mcents);
}

static bool tipjar_update(irc_t *irc, const char *channel, const char *target, int cents) {
    database_statement_t *stmt = database_statement_create("UPDATE TIPJAR SET CENTS = ? WHERE NAME = ?");
    if (!database_statement_bind(stmt, "is", cents, target) || !database_statement_complete(stmt))
        return false;
    tipjar_status(irc, channel, target);
    return true;
}

static bool tipjar_increment(irc_t *irc, const char *channel, const char *target, const char *victim) {
    int cents = tipjar_balance(target);
    if (cents == -1)
        return tipjar_account_open(target)
                    ? tipjar_increment(irc, channel, target, victim)
                    : false;
    irc_action(irc, channel, "grabs %s's wallet and steals some loose change", victim);
    return tipjar_update(irc, channel, target, cents + TIPJAR_CENTS);
}

static bool tipjar_decrement(irc_t *irc, const char *channel, const char *target) {
    int cents = tipjar_balance(target);
    if (cents == -1)
        return tipjar_account_open(target)
                    ? tipjar_decrement(irc, channel, target)
                    : false;
    return tipjar_update(irc, channel, target, cents - TIPJAR_CENTS);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t     *list   = strnsplit(message, " ", 2);
    const char *first  = list_shift(list);
    const char *second = list_shift(list);

    if (!access_range(irc, user, 4))
        return irc_write(irc, user, "You need level 4 or higher");
    if (!first || !strcmp(first, "-help"))
        return tipjar_help(irc, channel, user);
    if (!strcmp(first, "-revert")) {
        return second ? tipjar_decrement(irc, channel, second)
                      : tipjar_help(irc, channel, user);
    }
    return second ? tipjar_increment(irc, channel, first, second)
                  : tipjar_status(irc, channel, first);
}
