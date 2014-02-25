#include <module.h>
#include <string.h>
#include <stdio.h>

MODULE_TIMED(svn, 5);

#define SVN_DEPTH 5
#define SVN_URL   "svn://svn.icculus.org/redeclipse"
#define SVN_LINK  "http://redeclipse.net/svn/"

static bool svn_find(string_t *revision) {
    database_statement_t *check = database_statement_create("SELECT COUNT(*) FROM SVN WHERE REVISION = ?");
    if (!database_statement_bind(check, "S", revision))
        return false;

    database_row_t *row = database_row_extract(check, "i");
    if (!row)
        return false;

    return database_row_pop_integer(row) >= 1;
}

static void svn_add(svn_entry_t *e) {
    if (svn_find(e->revision))
        return;
    database_statement_t *add = database_statement_create("INSERT INTO SVN (REVISION, AUTHOR, MESSAGE) VALUES ( ?, ?, ? )");
    if (!add || !database_statement_bind(add, "SSS", e->revision, e->author, e->message))
        return;
    if (!database_statement_complete(add))
        return;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *ignore) {
    if (!ignore)
        return;

    /* Read SVN entries in and write them out to the channel */
    list_t *list = svnlog(SVN_URL, SVN_DEPTH);
    if (!list)
        return;

    svn_entry_t *e;
    while ((e = list_pop(list))) {
        if (svn_find(e->revision))
            continue;

        irc_write(irc, channel, "%s -> r%s -> %s%s -> %s",
            string_contents(e->author),
            string_contents(e->revision),
            SVN_LINK,
            string_contents(e->revision),
            string_contents(e->message)
        );

        svn_add(e);
    }
}