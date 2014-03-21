#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(faq);

static int faq_length(void) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM FAQ");
    if (!statement)
        return 0;

    database_row_t *row = database_row_extract(statement, "i");
    int count = database_row_pop_integer(row);
    if (!database_statement_complete(statement))
        return 0;

    return count;
}

static bool faq_find(const char *faq) {
    database_statement_t *statement = database_statement_create("SELECT COUNT(*) FROM FAQ WHERE NAME=?");
    if (!database_statement_bind(statement, "s", faq))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if (database_row_pop_integer(row) >= 1)
        return true;

    return false;
}

static void faq_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: faq [<fact> [nick]|<-stats>|<-add|-author|-concat|-forget> <fact> [<phrase>]]", user);
    irc_write(irc, channel, "%s: used -> (target): <phrase>", user);
}

static void faq_entry(irc_t *irc, const char *channel, const char *user, const char *faq) {
    database_statement_t *statement = database_statement_create("SELECT CONTENT FROM FAQ WHERE NAME = ? ORDER BY RANDOM() LIMIT 1");
    if (!database_statement_bind(statement, "s", faq))
        return;
    database_row_t *row = database_row_extract(statement, "s");
    if (!row)
        return irc_write(irc, channel, "%s: Sorry, \"%s\" is not in my FAQ list.", user, faq);

    const char *content = database_row_pop_string(row);
    if (!database_statement_complete(statement))
        return;

    database_request(irc, "FAQ");

    irc_write(irc, channel, "%s: %s = %s", user, faq, content);
}

static void faq_stats(irc_t *irc, const char *channel, const char *user, list_t *list) {
    const char *who = list_shift(list);
    if (!who) {
        int count   = faq_length();
        int request = database_request_count(irc, "FAQ");

        irc_write(irc, channel, "%s: faq stats -> %d faqs -> requested %d times", user, count, request);
    }
}

static void faq_add(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *faq     = list_shift(list);
    char *content = list_shift(list);

    if (!faq || !content)
        return;

    if (faq_find(faq))
        return irc_write(irc, channel, "%s: FAQ already exists.", user);

    database_statement_t *statement = database_statement_create("INSERT INTO FAQ (NAME, CONTENT, AUTHOR) VALUES ( ?, ?, ? )");
    if (!database_statement_bind(statement, "sss", faq, content, user))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, added faq: %s = %s", user, faq, content);
}

static void faq_concat(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *faq     = list_shift(list);
    char *content = list_shift(list);

    if (!faq || !content)
        return;

    if (!faq_find(faq)) {
        /* put them back and add it instead */
        list_push(list, content);
        list_push(list, faq);
        return faq_add(irc, channel, user, list);
    }

    database_statement_t *old = database_statement_create("SELECT CONTENT FROM FAQ WHERE NAME=?");
    if (!database_statement_bind(old, "s", faq))
        return;
    database_row_t *oldrow = database_row_extract(old, "s");
    if (!oldrow)
        return;
    const char *oldcontent = database_row_pop_string(oldrow);
    if (!database_statement_complete(old))
        return;

    string_t *string = string_format("%s OR %s", oldcontent, content);
    database_statement_t *statement = database_statement_create("UPDATE FAQ SET CONTENT=? WHERE NAME=?");
    if (!database_statement_bind(statement, "Ss", string, faq))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok updated FAQ: %s = %s", user, faq, string_contents(string));
}

static void faq_forget(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *faq = list_shift(list);
    if (!faq)
        return;

    if (!faq_find(faq))
        return irc_write(irc, channel, "%s: Sorry, could not find faq \"%s\"", user, faq);

    database_statement_t *statement = database_statement_create("DELETE FROM FAQ WHERE NAME = ?");
    if (!database_statement_bind(statement, "s", faq))
        return;
    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: Ok, removed - \"%s\" from FAQs", user, faq);
}

static void faq_author(irc_t *irc, const char *channel, const char *user, list_t *list) {
    char *faq = list_shift(list);
    if (!faq)
        return;

    if (!faq_find(faq))
        return irc_write(irc, channel, "%s: Sorry, could not find faq \"%s\"", user, faq);

    database_statement_t *statement = database_statement_create("SELECT AUTHOR FROM FAQ WHERE NAME = ?");
    if (!database_statement_bind(statement, "s", faq))
        return;
    database_row_t *row = database_row_extract(statement, "s");
    if (!row)
        return;

    const char *author = database_row_pop_string(row);

    if (!database_statement_complete(statement))
        return;

    irc_write(irc, channel, "%s: %s was written by %s", user, faq, author);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t     *list   = strnsplit(message, " ", 2);
    const char *method = list_shift(list);

    if (!strcmp(method, "-help"))    return faq_help(irc, channel, user);
    if (!strcmp(method, "-add"))     return faq_add(irc, channel, user, list);
    if (!strcmp(method, "-forget"))  return faq_forget(irc, channel, user, list);
    if (!strcmp(method, "-stats"))   return faq_stats(irc, channel, user, list);
    if (!strcmp(method, "-concat"))  return faq_concat(irc, channel, user, list);
    if (!strcmp(method, "-author"))  return faq_author(irc, channel, user, list);

    faq_entry(irc, channel, user, method);
}
