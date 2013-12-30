#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

struct database_statement_s;
struct database_row_s;
struct irc_s;
struct list_s;
struct list_iterator_s;
struct string_s;

typedef struct database_statement_s     database_statement_t;
typedef struct database_row_s           database_row_t;
typedef struct irc_s                    irc_t;
typedef struct list_s                   list_t;
typedef struct list_iterator_s          list_iterator_t;
typedef struct string_s                 string_t;

#define MODULE_DEFAULT(NAME) char module_name[]  = #NAME, module_match[] = #NAME
#define MODULE_ALWAYS(NAME)  char module_name[]  = #NAME, module_match[] = ""

list_t *module_irc_modules_list(irc_t *irc);
void *module_malloc(size_t bytes);
string_t *module_string_create(const char *input);
string_t *module_string_construct(void);
list_iterator_t *module_list_iterator_create(list_t *list);
list_t *module_list_create(void);
void module_list_push(list_t *list, void *element);
int module_getaddrinfo(const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result);
database_statement_t *module_database_statement_create(const char *string);
database_row_t *module_database_row_extract(database_statement_t *statement, const char *fields);
const char *module_database_row_pop_string(database_row_t *row);
char *module_strdup(const char *str);
list_t* module_strsplit(char *str, char *delim);

#define string_create              module_string_create
#define string_construct           module_string_construct
#define list_iterator_create       module_list_iterator_create
#define list_create                module_list_create
#define list_push                  module_list_push
#define getaddrinfo                module_getaddrinfo
#define database_statement_create  module_database_statement_create
#define database_row_extract       module_database_row_extract
#define database_row_pop_string    module_database_row_pop_string
#define irc_modules_list           module_irc_modules_list
#define malloc                     module_malloc
#define strdup                     module_strdup
#define strsplit                   module_strsplit

void list_iterator_reset(list_iterator_t *iterator);
bool list_iterator_end(list_iterator_t *iterator);
void *list_iterator_next(list_iterator_t *iterator);
void *list_pop(list_t *list);
size_t list_length(list_t *list);
void string_catf(string_t *string, const char *fmt, ...);
void string_destroy(string_t *string);
size_t string_length(string_t *string);
bool string_empty(string_t *string);
char *const string_contents(string_t *string);
bool database_statement_complete(database_statement_t *statement);
bool database_statement_bind(database_statement_t *statement, const char *mapping, ...);
database_row_t *database_row_extract(database_statement_t *statement, const char *fields);
const char *database_row_pop_string(database_row_t *row);
int  database_row_pop_integer(database_row_t *row);
bool database_request(irc_t *instance, const char *table);
int database_request_count(irc_t *instance, const char *table);
bool irc_modules_reload(irc_t *irc, const char *name);
bool irc_modules_add(irc_t *irc, const char *file);
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
const char *irc_nick(irc_t *irc);
list_t *irc_modules(irc_t *irc);

#endif
