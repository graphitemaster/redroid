#ifndef REDROID_REGEXPR_HDR
#define REDROID_REGEXPR_HDR
#include <stdbool.h>

typedef struct hashtable_s  regexpr_cache_t;
typedef struct regexpr_s    regexpr_t;

typedef struct {
    int soff;
    int eoff;
} regexpr_match_t;

/* regular expression management */
regexpr_t *regexpr_create(regexpr_cache_t *cache, const char *string, bool icase);
void regexpr_destroy(regexpr_t *regexpr);
bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array);
void regexpr_execute_destroy(regexpr_match_t *array);

/* regular expression cache */
void regexpr_cache_destroy(regexpr_cache_t *cache);
regexpr_cache_t *regexpr_cache_create(void);

#endif
