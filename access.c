#include "irc.h"
#include "access.h"
/* Access control for the bot */

static bool access_clamp(int value) {
    return !!(value >= ACCESS_MIN && value <= ACCESS_MAX);
}

bool access_level(irc_t *irc, const char *channel, const char *target, int *level) {
    database_statement_t *statement =
        database_statement_create(irc->database, "SELECT ACCESS FROM ACCESS WHERE NAME=? AND CHANNEL=?");

    if (!database_statement_bind(statement, "ss", target, channel))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if (!row)
        return false;

    int extract = database_row_pop_integer(row);
    if (!database_statement_complete(statement)) {
        database_row_destroy(row);
        return false;
    }

    *level = extract;
    database_row_destroy(row);
    return true;
}

access_t access_insert(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    if (!access_clamp(level))
        return ACCESS_BADRANGE;
    /* Prevent invoker from adding if they're not part of the access list themselves */
    int invokelevel = 0;
    if (!access_level(irc, channel, invoke, &invokelevel))
        return ACCESS_NOEXIST_INVOKE;
    /* Prevent double add */
    if (access_level(irc, channel, target, &(int){0}))
        return ACCESS_EXISTS;
    /* Prevent access control higher than invokers */
    if (invokelevel < ACCESS_CONTROL)
        return ACCESS_DENIED;

    database_statement_t *statement =
        database_statement_create(irc->database,
            "INSERT INTO ACCESS(CHANNEL, NAME, ACCESS) "
            "VALUES (?, ?, ?)");

    if (!database_statement_bind(statement, "ssi", channel, target, level))
        return ACCESS_FAILED;
    if (!database_statement_complete(statement))
        return ACCESS_FAILED;
    return ACCESS_SUCCESS;
}

access_t access_remove(irc_t *irc, const char *channel, const char *target, const char *invoke) {
    int invokelevel = 0;
    int targetlevel = 0;

    if (!access_level(irc, channel, invoke, &invokelevel))
        return ACCESS_NOEXIST_INVOKE;
    if (!access_level(irc, channel, target, &targetlevel))
        return ACCESS_NOEXIST_TARGET;
    if (invokelevel < ACCESS_CONTROL || targetlevel >= invokelevel)
        return ACCESS_DENIED;

    database_statement_t *statement =
        database_statement_create(irc->database,
            "DELETE FROM ACCESS WHERE NAME=? AND CHANNEL=?");

    if (!database_statement_bind(statement, "ss", target, channel))
        return ACCESS_FAILED;
    if (!database_statement_complete(statement))
        return ACCESS_FAILED;
    return ACCESS_SUCCESS;
}

access_t access_change(irc_t *irc, const char *channel, const char *target, const char *invoke, int level) {
    if (!access_clamp(level))
        return ACCESS_BADRANGE;

    int invokelevel = 0;
    int targetlevel = 0;

    if (!access_level(irc, channel, invoke, &invokelevel))
        return ACCESS_NOEXIST_INVOKE;
    if (!access_level(irc, channel, target, &targetlevel))
        return ACCESS_NOEXIST_TARGET;

    /* If the target has >= senority than the invoker then deny */
    if (targetlevel > invokelevel)
        return ACCESS_DENIED;
    /* If the level is greater than that of the invokers then deny */
    if (level > invokelevel)
        return ACCESS_DENIED;

    /* Otherwise we just change their access */
    database_statement_t *statement =
        database_statement_create(irc->database, "UPDATE ACCESS SET ACCESS=? WHERE NAME=? AND CHANNEL=?");
    if (!database_statement_bind(statement, "iss", level, target, channel))
        return ACCESS_FAILED;
    if (!database_statement_complete(statement))
        return ACCESS_FAILED;
    return ACCESS_SUCCESS;
}

bool access_check(irc_t *irc, const char *channel, const char *target, int check) {
    int level = 0;
    if (!access_level(irc, channel, target, &level))
        return false;
    return !!(level == check);
}

bool access_range(irc_t *irc, const char *channel, const char *target, int check) {
    int level = 0;
    if (!access_level(irc, channel, target, &level))
        return false;
    return !!(level >= check);
}
