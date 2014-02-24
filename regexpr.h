#ifndef REDROID_REGEXPR_HDR
#define REDROID_REGEXPR_HDR
#include "hashtable.h"

typedef struct regexpr_s regexpr_t;

typedef struct {
    size_t soff;
    size_t eoff;
} regexpr_match_t;

/* regular expression management */
regexpr_t *regexpr_create(hashtable_t *cache, const char *string, bool icase);
void regexpr_destroy(regexpr_t *regexpr);
bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array);
void regexpr_execute_destroy(regexpr_match_t *array);

/* regular expression cache */
void regexpr_cache_destroy(hashtable_t *cache);
hashtable_t *regexpr_cache_create(void);

#endif
