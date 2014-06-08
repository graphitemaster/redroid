#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <netdb.h>

/* Types */
typedef struct regexpr_s                regexpr_t;
typedef struct database_statement_s     database_statement_t;
typedef struct database_row_s           database_row_t;
typedef struct irc_s                    irc_t;
typedef struct list_s                   list_t;
typedef struct list_iterator_s          list_iterator_t;
typedef struct string_s                 string_t;
typedef struct hashtable_s              hashtable_t;

typedef struct {
    int soff;
    int eoff;
} regexpr_match_t;

typedef struct {
    string_t *message;
    string_t *author;
    string_t *revision;
} svn_entry_t;

/* Module defining macros */
#define MODULE_DEFAULT(NAME)   char module_name[] = #NAME, module_match[] = #NAME
#define MODULE_ALWAYS(NAME)    char module_name[] = #NAME, module_match[] = ""

#define MODULE_TIMED(NAME, INTERVAL) \
    MODULE_ALWAYS(NAME);             \
    int module_interval = INTERVAL

#define MODULE_GC_CALL(NAME) ({                \
        extern __typeof__(NAME) module_##NAME; \
        module_##NAME;                         \
    })

/* IRC API */
void irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
void irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
void irc_part(irc_t *irc, const char *channel);
void irc_join(irc_t *irc, const char *channel);

static inline list_t *irc_modules_loaded(irc_t *irc) {
    return MODULE_GC_CALL(irc_modules_loaded)(irc);
}
static inline list_t *irc_modules_enabled(irc_t *irc, const char *channel) {
    return MODULE_GC_CALL(irc_modules_enabled)(irc, channel);
}
static inline hashtable_t *irc_modules_config(irc_t *irc, const char *channel) {
    return MODULE_GC_CALL(irc_modules_config)(irc, channel);
}
static inline list_t *irc_users(irc_t *irc, const char *channel) {
    return MODULE_GC_CALL(irc_users)(irc, channel);
}
static inline list_t *irc_channels(irc_t *irc) {
    return MODULE_GC_CALL(irc_channels)(irc);
}
const char *irc_nick(irc_t *irc);
const char *irc_topic(irc_t *irc, const char *channel);
const char *irc_pattern(irc_t *irc, const char *newpattern);

typedef enum {
    MODULE_STATUS_REFERENCED,
    MODULE_STATUS_SUCCESS,
    MODULE_STATUS_FAILURE,
    MODULE_STATUS_ALREADY,
    MODULE_STATUS_NONEXIST
} module_status_t;

module_status_t irc_modules_add(irc_t *irc, const char *file);
module_status_t irc_modules_reload(irc_t *irc, const char *name);
module_status_t irc_modules_unload(irc_t *irc, const char *channel, const char *name, bool force);
module_status_t irc_modules_disable(irc_t *irc, const char *channel, const char *name);
module_status_t irc_modules_enable(irc_t *irc, const char *channel, const char *name);

/* string API */
static inline string_t *string_create(const char *input) {
    return MODULE_GC_CALL(string_create)(input);
}
static inline string_t *string_format(const char *input, ...) {
    extern string_t *string_vformat(const char *, va_list);
    va_list va;
    va_start(va, input);
    string_t *string = MODULE_GC_CALL(string_vformat)(input, va);
    va_end(va);
    return string;
}
static inline string_t *string_construct(void) {
    return MODULE_GC_CALL(string_construct)();
}
void string_catf(string_t *string, const char *fmt, ...);
size_t string_length(string_t *string);
bool string_empty(string_t *string);
char *string_contents(string_t *string);

static inline char *strdup(const char *string) {
    return MODULE_GC_CALL(strdup)(string);
}
static inline list_t *strsplit(const char *string, const char *delimiter) {
    return MODULE_GC_CALL(strsplit)(string, delimiter);
}
static inline list_t *strnsplit(const char *string, const char *delimiter, size_t count) {
    return MODULE_GC_CALL(strnsplit)(string, delimiter, count);
}
static inline char *strdur(unsigned long long dur) {
    return MODULE_GC_CALL(strdur)(dur);
}

/* List API */
void list_iterator_reset(list_iterator_t *iterator);
bool list_iterator_end(list_iterator_t *iterator);
void *list_iterator_next(list_iterator_t *iterator);
void *list_iterator_prev(list_iterator_t *iterator);
void *list_pop(list_t *list);
void *list_shift(list_t *list);
size_t list_length(list_t *list);
void list_sort(list_t *list, bool (*predicate)(const void *, const void *));
void *list_search(list_t *list, const void *pass, bool (*predicate)(const void *, const void *));
void *list_at(list_t *list, size_t index);
void list_push(list_t *list, void *element);
static inline list_iterator_t *list_iterator_create(list_t *list) {
    return MODULE_GC_CALL(list_iterator_create)(list);
}

static inline list_t *list_create(void) {
    return MODULE_GC_CALL(list_create)();
}

/* Hashtable API */
void *hashtable_find(hashtable_t *hashtable, const char *key);
bool hashtable_insert(hashtable_t *hashtable, const char *key, void *value);

/* Database API */
bool database_statement_complete(database_statement_t *statement);
bool database_statement_bind(database_statement_t *statement, const char *mapping, ...);
int  database_row_pop_integer(database_row_t *row);
bool database_request(irc_t *instance, const char *table);
int database_request_count(irc_t *instance, const char *table);
static inline database_statement_t *database_statement_create(const char *string) {
    return MODULE_GC_CALL(database_statement_create)(string);
}
static inline database_row_t *database_row_extract(database_statement_t *statement, const char *fields) {
    return MODULE_GC_CALL(database_row_extract)(statement, fields);
}
static inline const char *database_row_pop_string(database_row_t *row) {
    return MODULE_GC_CALL(database_row_pop_string)(row);
}

/* Regular expression API */
#define regexpr_match_invalid(X) \
    ((X).soff == -1 || ((X).eoff == -1))
static inline regexpr_t *regexpr_create(const char *string, bool icase) {
    return MODULE_GC_CALL(regexpr_create)(string, icase);
}
static inline bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    return MODULE_GC_CALL(regexpr_execute)(expr, string, nmatch, array);
}

/* Random number generation API */
static inline uint32_t urand(void) {
    return MODULE_GC_CALL(urand)();
}

static inline double drand(void) {
    return MODULE_GC_CALL(drand)();
}

/* Redroid utility API */
void redroid_restart(irc_t *irc, const char *channel, const char *user);
void redroid_shutdown(irc_t *irc, const char *channel, const char *user);
void redroid_recompile(irc_t *irc, const char *channel, const char *user);
void redroid_daemonize(irc_t *irc, const char *channel, const char *user);

const char *build_info(void);

/* Access control API */
#define ACCESS_CONTROL 4

typedef enum {
    ACCESS_NOEXIST_TARGET,
    ACCESS_NOEXIST_INVOKE,
    ACCESS_EXISTS,
    ACCESS_DENIED,
    ACCESS_SUCCESS,
    ACCESS_FAILED,
    ACCESS_BADRANGE
} access_t;

bool     access_range (irc_t *irc, const char *target, int check);
bool     access_check (irc_t *irc, const char *target, int check);
bool     access_level (irc_t *irc, const char *target, int *level);
access_t access_remove(irc_t *irc, const char *target, const char *invoke);
access_t access_insert(irc_t *irc, const char *target, const char *invoke, int level);
access_t access_change(irc_t *irc, const char *target, const char *invoke, int level);

/* Misc */
static inline void *malloc(size_t size) {
    return MODULE_GC_CALL(malloc)(size);
}
static inline int getaddrinfo_(const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    return MODULE_GC_CALL(getaddrinfo)(mode, service, hints, result);
}
static inline list_t *svnlog(const char *url, size_t depth) {
    return MODULE_GC_CALL(svnlog)(url, depth);
}

#define getaddrinfo getaddrinfo_
#endif
