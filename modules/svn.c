#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

MODULE_TIMED(svn, 5);

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
    if (!database_statement_bind(add, "SSS", e->revision, e->author, e->message))
        return;
    if (!database_statement_complete(add))
        return;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    (void)user; /* ignored */
    (void)message; /* ignored */


    hashtable_t *get;
    if (!(get = irc_modules_config(irc, channel)))
        return;

    const char *host;
    const char *url;
    const char *depth;
    const char *template;

    if (!(host     = hashtable_find(get, "host")))     return;
    if (!(depth    = hashtable_find(get, "depth")))    return;
    if (!(template = hashtable_find(get, "template"))) return;
    if (!(url      = hashtable_find(get, "url")))      return;

    //irc_write(irc, channel, "[[RAW:TIME(svn.so)]] @ dispatch [#%s:%s:%s:%s]", url, depth, template, url);

    /* Read SVN entries in and write them out to the channel */
    list_t *list = svnlog(host, atoi(depth));
    if (!list)
        return;

    svn_entry_t *e;
    while ((e = list_pop(list))) {
        if (svn_find(e->revision))
            continue;

        /* Populate the template */
        string_t *message = string_create(template);
        string_replace(message, "{{revision}}", string_contents(e->revision));
        string_replace(message, "{{author}}", string_contents(e->author));
        string_replace(message, "{{message}}", string_contents(e->message));
        string_t *format = string_format("%s/%s", url, string_contents(e->revision));
        string_replace(message, "{{url}}", string_contents(format));

        irc_write(irc, channel, "%s", string_contents(message));
        svn_add(e);
    }
}
