#include <module.h>

MODULE_DEFAULT(kitten);

typedef struct {
    const char *killed;
    const char *object;
    const char *style;
} kitten_t;

static kitten_t *kitten_method(void) {
    database_statement_t *killed = database_statement_create("SELECT * FROM KILLED ORDER BY RANDOM() LIMIT 1");
    database_statement_t *object = database_statement_create("SELECT * FROM OBJECT ORDER BY RANDOM() LIMIT 1");
    database_statement_t *style  = database_statement_create("SELECT * FROM STYLE ORDER BY RANDOM() LIMIT 1");

    if (!killed || !object || !style)
        return NULL;

    database_row_t *krow = database_row_extract(killed, "s");
    database_row_t *orow = database_row_extract(object, "s");
    database_row_t *srow = database_row_extract(style,  "s");

    if (!krow || !orow || !srow)
        return NULL;

    kitten_t *kitten = malloc(sizeof(*kitten));
    if (!(kitten->killed = database_row_pop_string(krow))
    ||  !(kitten->object = database_row_pop_string(orow))
    ||  !(kitten->style  = database_row_pop_string(srow)))
        return NULL;

    if (!database_statement_complete(killed)
    ||  !database_statement_complete(object)
    ||  !database_statement_complete(style))
        return NULL;

    return kitten;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    database_request(irc, "KITTENS");
    int count = database_request_count(irc, "KITTENS");
    if (message) {
        kitten_t *kill = kitten_method();
        irc_write(irc, channel, "A kitten was %s with %s by %s %s - %d kittens have now been killed in the name of noobs",
            kill->killed,
            kill->object,
            message,
            kill->style,
            count
        );

        if (count % 1000 == 0)
            irc_write(irc, channel, "This was the %dth kitten to be sacrificed", count);
    } else {
        irc_write(irc, channel, "%d kittens have been killed in the name of noobs", count);
    }
}
