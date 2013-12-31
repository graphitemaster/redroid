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
// The cost of executing a regular expression is far less than that of
// creating one and since there is only a few modules that depend on
// regular expressions having a fixed static size cache if sufficent
// enough to compensate for regular expression construction. It should
// be noted however that when the cache gets full the LEAST hot, i.e
// cold (least commonly invoked) regular expressions will be erased
// from the cache
//
// Each new regular expression gets added to the tail end of the cache
// which is considered the cold end. Hot regular expression objects are
// sorted on hotness-update to exist at the head of the cache.
//
// Right now a number of sixteen is a fair-enough static size for the
// cache. When the cache reaches this value in elements the cold half
// of the cache is erased, i.e this number / 2 is erased from the tail
// end of the cache.
//
#define REGEXPR_CACHE_SIZE 16

struct regexpr_s {
    regex_t reg;
    char   *match;
    size_t  hot;
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
    //
    // Throw away anything that isn't hot cached, i.e isn't used often,
    // i.e remove the cold compiled expressions. Hotter cached elements
    // are at the head of the cache.
    //
    if (list_length(cache->cache) >= REGEXPR_CACHE_SIZE)
        for (size_t i = 0; i < (REGEXPR_CACHE_SIZE >> 1); i++)
            regexpr_destroy(list_pop(cache->cache));

    list_push(cache->cache, expr);
}

static bool regexpr_cache_sort(const void *a, const void *b) {
    const regexpr_t *ra = a;
    const regexpr_t *rb = b;

    return !!(ra->hot < rb->hot);
}

static void regexpr_cache_hotter(regexpr_cache_t *cache, regexpr_t *find) {
    //
    // Make the current expression hotter in the cache and force a restart
    // of the cache.
    //
    find->hot++;
    //list_sort(cache->cache, &regexpr_cache_sort);
}

// regular expression management
regexpr_t *regexpr_create(regexpr_cache_t *cache, const char *string, bool icase) {
    //
    // save on recompiling complex regular expressions if they're in cache.
    // This helps when a regex using module is invoked multiple times in succession,
    // which is quite common in an IRC channel.
    //
    regexpr_t *find;
    if ((find = regexpr_cache_find(cache, string))) {
        //
        // Make the current regular expression hotter in the cache since
        // it's used more often as a result.
        //
        regexpr_cache_hotter(cache, find);
        return find;
    }

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
