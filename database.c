#include "database.h"
#include "string.h"
#include "irc.h"

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sqlite3.h>

typedef struct database_row_data_s database_row_data_t;
typedef struct database_s          database_t;

struct database_statement_s {
    sqlite3_stmt    *statement;
    char            *string;
};

struct database_s {
    hashtable_t     *statements;
    sqlite3         *handle;
};

struct database_row_data_s {
    bool                 integer;
    database_row_data_t *next;

    union {
        void            *data;
        int              ival;
    };
};

struct database_row_s {
    database_row_data_t *head;
    database_row_data_t *tail;
};

static database_row_data_t *database_row_data_create(void) {
    return calloc(1, sizeof(database_row_data_t));
}

static void database_row_data_destroy(database_row_data_t *data) {
    free(data);
}

static database_row_t *database_row_create(void) {
    database_row_t *row = malloc(sizeof(*row));
    row->head = database_row_data_create();
    row->tail = row->head;
    return row;
}

void database_row_destroy(database_row_t *row) {
    database_row_data_destroy(row->head);
    free(row);
}

static void database_row_push_string(database_row_t *row, char *string) {
    row->tail->next    = database_row_data_create();
    row->tail->data    = string;
    row->tail->integer = false;
    row->tail          = row->tail->next;
}

static void database_row_push_integer(database_row_t *row, int value) {
    row->tail->next    = database_row_data_create();
    row->tail->ival    = value;
    row->tail->integer = true;
    row->tail          = row->tail->next;
}

static void database_statement_destroy(database_statement_t *statement) {
    sqlite3_finalize(statement->statement);
    free(statement->string);
    free(statement);
}

database_statement_t *database_statement_create(database_t *database, const char *string) {
    database_statement_t *find = hashtable_find(database->statements, string);
    if (find) {
        if (sqlite3_reset(find->statement) != SQLITE_OK) {
            database_statement_destroy(find);
            hashtable_remove(database->statements, string);
            goto create;
        }
        return find;
    }

create:
    find = malloc(sizeof(*find));
    if (sqlite3_prepare_v2(database->handle, string, -1, &find->statement, NULL) != SQLITE_OK) {
        free(find);
        return NULL;
    }
    find->string = strdup(string);
    hashtable_insert(database->statements, string, find);
    return find;
}

bool database_statement_complete(database_statement_t *statement) {
    /* I know it's pretty ugly but sqlite3 hates me */
    int trystmt = 0;
    while ((trystmt = sqlite3_step(statement->statement)) == SQLITE_BUSY)
        ;
    return !!(trystmt == SQLITE_DONE);
}

bool database_statement_bind(database_statement_t *statement, const char *mapping, ...) {
    if (!statement)
        return false;

    va_list va;
    va_start(va, mapping);

    size_t index = 1;
    for (const char *entry = mapping; *entry; entry++, index++) {
        switch (*entry) {
            case 's':
                if (sqlite3_bind_text(statement->statement, index, va_arg(va, char *), -1, NULL) != SQLITE_OK)
                    goto error;
                break;

            case 'S':
                if (sqlite3_bind_text(statement->statement, index, string_contents(va_arg(va, void *)), -1, NULL) != SQLITE_OK)
                    goto error;
                break;

            case 'i':
                if (sqlite3_bind_int(statement->statement, index, va_arg(va, int)) != SQLITE_OK)
                    goto error;
                break;
        }
    }

    va_end(va);
    return true;

error:
    va_end(va);
    return false;
}

database_row_t *database_row_extract(database_statement_t *statement, const char *fields) {
    if (!statement || sqlite3_step(statement->statement) != SQLITE_ROW)
        return NULL;

    database_row_t *row = database_row_create();
    size_t index = 0;
    for (const char *field = fields; *field; field++, index++) {
        switch (*field) {
            case 's':
                database_row_push_string(row, (char *)sqlite3_column_text(statement->statement, index));
                break;

            case 'i':
                database_row_push_integer(row, sqlite3_column_int(statement->statement, index));
                break;
        }
    }

    return row;
}

const char *database_row_pop_string(database_row_t *row) {
    if (!row->head->data)
        return NULL;

    database_row_data_t *temp = row->head->next;
    const char *ret = strdup(row->head->data);
    database_row_data_destroy(row->head);
    row->head = temp;
    return ret;
}

int database_row_pop_integer(database_row_t *row) {
    if (!row->head->integer)
        return 0;

    database_row_data_t *temp = row->head->next;
    int value = row->head->ival;
    database_row_data_destroy(row->head);
    row->head = temp;
    return value;
}

database_t *database_create(const char *file) {
    database_t *database = malloc(sizeof(*database));
    if (sqlite3_open(file, &database->handle)) {
        free(database);
        return NULL;
    }
    database->statements = hashtable_create(256);
    return database;
}

void database_destroy(database_t *database) {
    /* Clear the statement cache */
    hashtable_foreach(database->statements, NULL, &database_statement_destroy);
    hashtable_destroy(database->statements);

    sqlite3_close(database->handle);
    free(database);
}

/* Request table management */
int database_request_count(irc_t *instance, const char *table) {
    database_statement_t *statement = database_statement_create(instance->database, "SELECT COUNT FROM REQUESTS WHERE NAME=?");
    if (!statement)
        return -1;

    if (!database_statement_bind(statement, "s", table))
        return -1;

    database_row_t *row = database_row_extract(statement, "i");
    if (!row)
        return -1;

    int count = database_row_pop_integer(row);

    if (!database_statement_complete(statement)) {
        database_row_destroy(row);
        return -1;
    }

    database_row_destroy(row);
    return count;
}

bool database_request(irc_t *instance, const char *table) {
    int oldcount = database_request_count(instance, table);
    if (oldcount == -1)
        return false;

    database_statement_t *statement = database_statement_create(instance->database, "UPDATE REQUESTS SET COUNT=? WHERE NAME=?");

    if (!statement)
        return false;

    oldcount++;

    if (!database_statement_bind(statement, "is", oldcount, table))
        return false;

    if (!database_statement_complete(statement))
        return false;

    return true;
}
