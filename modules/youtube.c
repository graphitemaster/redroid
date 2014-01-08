#include <module.h>
#include <string.h>
#include <time.h>

MODULE_ALWAYS(youtube);

#define NETHERS 4000

typedef struct {
    const char *user;
    const char *chan;
    int         count;
    int         timestamp;
} youtube_t;

static bool youtube_find(const char *id, youtube_t *data) {
    database_statement_t *check = database_statement_create("SELECT AUTHOR, CHANNEL, TIMESTAMP, COUNT FROM YOUTUBE WHERE ID = ?");
    if (!check || !database_statement_bind(check, "s", id))
        return false;

    database_row_t *row = database_row_extract(check, "ssii");
    if (!row)
        return false;

    data->user      = database_row_pop_string(row);
    data->chan      = database_row_pop_string(row);
    data->timestamp = database_row_pop_integer(row);
    data->count     = database_row_pop_integer(row);

    if (!database_statement_complete(check))
        return false;
    return true;
}

static bool youtube_update(const char *id, youtube_t *data) {
    data->count++;
    database_statement_t *update = database_statement_create("UPDATE YOUTUBE SET COUNT = ?, AUTHOR = ?, TIMESTAMP = ? WHERE ID = ?");
    if (!update || !database_statement_bind(update, "isis", data->count, data->user, data->timestamp, id))
        return false;
    if (!database_statement_complete(update))
        return false;
    return true;
}

static bool youtube_add(const char *id, youtube_t *data) {
    database_statement_t *add = database_statement_create("INSERT INTO YOUTUBE VALUES(?, ?, ?, ?, ?)");
    if (!add || !database_statement_bind(add, "sssii", data->user, data->chan, id, data->timestamp, data->count))
        return false;
    if (!database_statement_complete(add))
        return false;
    return true;
}

static int youtube_count(void) {
    database_statement_t *count = database_statement_create("SELECT SUM(COUNT) FROM YOUTUBE");
    if (!count)
        return 0;
    database_row_t *row = database_row_extract(count, "i");
    if (!row)
        return 0;
    return database_row_pop_integer(row);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (strstr(message, "-stats") == &message[0])
        goto last;

    regexpr_t *regex = regexpr_create("(https?://)?(www\\.)?youtu(\\.be/|be\\.com/watch.*[?&]v=)([^&]+)", false);
    if (!regex)
        return;

    regexpr_match_t *matches;
    if (!regexpr_execute(regex, message, 5, &matches))
        return;

    regexpr_match_t videoid = matches[4];
    if (regexpr_match_invalid(videoid))
        return;

    char *id = strdup(message + videoid.soff);
    id[videoid.eoff - videoid.soff] = 0;
    if (strchr(id, ' '))
        *strchr(id, ' ')=0;

    youtube_t data;
    if (youtube_find(id, &data)) {
        irc_write(irc, channel, "%s: I've seen that youtube link %d %s before, last was %d by %s on %s",
            user, data.count, (data.count == 1) ? "time" : "times",
            data.timestamp, data.user, (!strcmp(data.chan, channel)) ? "this channel" : data.chan
        );
        // update
        data.user      = user;
        data.timestamp = time(NULL);
        youtube_update(id, &data);
    } else {
        data.user      = user;
        data.count     = 1;
        data.timestamp = time(NULL);
        data.chan      = channel;

        youtube_add(id, &data);
    }

    int count;
last:
    count = youtube_count();
    irc_write(irc, channel,
        "%s: A total of %d %s been spammed in my presence, only %d more to go until graphite waxes his nethers.",
        user, count, (count == 1) ? "link has" : "links have",
        NETHERS - count
    );
}
