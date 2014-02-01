#include "regexpr.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

struct regexpr_s {
    regex_t reg;
    char   *match;
};

struct regexpr_cache_s {
    list_t *cache;
};

static bool regexpr_cache_find(const void *a, const void *b) {
    const regexpr_t *expr = a;
    if (!strcmp(expr->match, (const char *)b))
        return true;
    return false;
}

void regexpr_cache_destroy(regexpr_cache_t *cache) {
    list_iterator_t *it = list_iterator_create(cache->cache);

    while (!list_iterator_end(it))
        regexpr_destroy(list_iterator_next(it));

    list_iterator_destroy(it);
    list_destroy(cache->cache);
    free(cache);
}

regexpr_cache_t *regexpr_cache_create(void) {
    regexpr_cache_t *cache = malloc(sizeof(*cache));
    cache->cache = list_create();
    return cache;
}

static void regexpr_cache_insert(regexpr_cache_t *cache, regexpr_t *expr) {
    list_push(cache->cache, expr);
}

// regular expression management
regexpr_t *regexpr_create(regexpr_cache_t *cache, const char *string, bool icase) {
    //
    // save on recompiling complex regular expressions if they're in cache.
    // This helps when a regex using module is invoked multiple times in succession,
    // which is quite common in an IRC channel.
    //
    regexpr_t *find;
    if ((find = list_search(cache->cache, &regexpr_cache_find, string)))
        return find;

    regexpr_t *next = malloc(sizeof(*next));

    if (regcomp(&next->reg, string, REG_EXTENDED | ((icase) ? REG_ICASE : 0)) != 0) {
        free(next);
        return NULL;
    }

    next->match = strdup(string);

    //
    // New entries won't have any initial hotness and will literally be
    // at the tail of the list.
    //
    regexpr_cache_insert(cache, next);
    return next;
}

void regexpr_destroy(regexpr_t *regexpr) {
    regfree(&regexpr->reg);
    free(regexpr->match);
    free(regexpr);
}

// regular expression execution/result
bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    regmatch_t *temp = NULL;
    if (nmatch)
        temp = malloc(sizeof(regmatch_t) * nmatch);

    if (regexec(&expr->reg, string, nmatch, temp, 0) != 0) {
        free(temp);
        return false;
    }

    if (nmatch) {
        *array = malloc(sizeof(regexpr_match_t) * nmatch);
        for (size_t i = 0; i < nmatch; i++) {
            (*array)[i].soff = temp[i].rm_so;
            (*array)[i].eoff = temp[i].rm_eo;
        }
        free(temp);
    }

    return true;
}

void regexpr_execute_destroy(regexpr_match_t *array) {
    free(array);
}
