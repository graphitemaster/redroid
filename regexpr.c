#include "regexpr.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

struct regexpr_s {
    regex_t reg;
    char   *match;
};

void regexpr_cache_destroy(hashtable_t *cache) {
    hashtable_foreach(cache, NULL, &regexpr_destroy);
    hashtable_destroy(cache);
}

hashtable_t *regexpr_cache_create(void) {
    return hashtable_create(256);
}

/* regular expression management */
regexpr_t *regexpr_create(hashtable_t *cache, const char *string, bool icase) {
    /*
     * save on recompiling complex regular expressions if they're in cache.
     * This helps when a regex using module is invoked multiple times in succession,
     * which is quite common in an IRC channel.
     */
    regexpr_t *find;
    if ((find = hashtable_find(cache, string)))
        return find;

    regexpr_t *next = malloc(sizeof(*next));

    if (regcomp(&next->reg, string, REG_EXTENDED | ((icase) ? REG_ICASE : 0)) != 0) {
        free(next);
        return NULL;
    }

    next->match = strdup(string);
    hashtable_insert(cache, string, next);
    return next;
}

void regexpr_destroy(regexpr_t *regexpr) {
    regfree(&regexpr->reg);
    free(regexpr->match);
    free(regexpr);
}

/* regular expression execution/result */
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
