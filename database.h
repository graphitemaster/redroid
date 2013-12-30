#ifndef REDROID_DATABASE_HDR
#define REDROID_DATABASE_HDR
#include <stdbool.h>

struct irc_s;
struct database_statement_s;
struct database_row_s;
struct database_s;

typedef struct database_statement_s database_statement_t;
typedef struct database_row_s       database_row_t;
typedef struct database_s           database_t;

database_statement_t *database_statement_create(database_t *database, const char *string);
void                  database_statement_destroy(database_statement_t *statement);
bool                  database_statement_complete(database_statement_t *statement);
bool                  database_statement_bind(database_statement_t *statement, const char *mapping, ...);

database_row_t       *database_row_extract(database_statement_t *statement, const char *fields);
void                  database_row_destroy(database_row_t *row);
const char           *database_row_pop_string(database_row_t *row);
int                   database_row_pop_integer(database_row_t *row);


database_t           *database_create(const char *file);
void                  database_destroy(database_t *database);

bool                  database_request(struct irc_s *instance, const char *table);
int                   database_request_count(struct irc_s *instance, const char *table);

#endif
