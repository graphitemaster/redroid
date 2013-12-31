#include "regexpr.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

//
// MODULE_ALWAYS functions are always entered regardless of their match.
// one could argue that regular expression would go on THAT. But that
// doesn't solve the issue with something still being entered constantly
// that may not depend on that specific match. It's easy to compile a
// regular expression each time that moudle is entered, which is once
// for every line from an IRC server. This isn't efficent and it also
// lends itself to being wasteful even for when the regular expression
// is only created once for the process. The bigger problem is when users
// on an IRC channel decide to play with the module and spam it, forcing
// it to create and executes its regular expression for every invocation.
// This provides a hot/cold cache of memory-managed compiled regular
// expressions to deal with this.
//

struct regexpr_s {
    regex_t reg;
    char   *match;
};

struct regexpr_cache_s {
    list_iterator_t *iterator;
    list_t          *cache;
};

static regexpr_t *regexpr_cache_find(regexpr_cache_t *cache, const char *string) {
    list_iterator_t *it = cache->iterator;
    list_iterator_reset(it);

    while (!list_iterator_end(it)) {
        regexpr_t *expr = list_iterator_next(it);
        if (!strcmp(expr->match, string))
            return expr;
    }

    return NULL;
}

void regexpr_cache_destroy(regexpr_cache_t *cache) {
    list_iterator_t *it = cache->iterator;
    list_iterator_reset(it);

    while (!list_iterator_end(it))
        regexpr_destroy(list_iterator_next(it));

    list_iterator_destroy(it);
    list_destroy(cache->cache);
    free(cache);
}

regexpr_cache_t *regexpr_cache_create(void) {
    regexpr_cache_t *cache = malloc(sizeof(*cache));

    cache->cache    = list_create();
    cache->iterator = list_iterator_create(cache->cache);

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
    if ((find = regexpr_cache_find(cache, string)))
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
bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, list_t **list) {
    regmatch_t *temp = NULL;
    if (nmatch)
        temp = malloc(sizeof(regmatch_t) * nmatch);

    if (regexec(&expr->reg, string, nmatch, temp, 0) != 0) {
        free(temp);
        return false;
    }

    if (nmatch) {
        *list = list_create();
        for (size_t i = 0; i < nmatch; i++) {
            const regexpr_match_t match = {
                .soff = temp[i].rm_so,
                .eoff = temp[i].rm_eo
            };
            list_push(*list, memcpy(malloc(sizeof(regexpr_match_t)), &match, sizeof(regexpr_match_t)));
        }
        free(temp);
    }

    return true;
}

void regexpr_execute_destroy(list_t *list) {
    list_iterator_t *it = list_iterator_create(list);
    while (!list_iterator_end(it))
        free(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(list);
}
