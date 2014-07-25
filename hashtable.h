#ifndef REDROID_HASHTABLE_HDR
#define REDROID_HASHTABLE_HDR
#include <stdbool.h>
#include <stddef.h>

#include "default.h"

typedef struct hashtable_s hashtable_t;

hashtable_t *hashtable_create(size_t size);

void hashtable_destroy(hashtable_t *hashtable);

void hashtable_insert(hashtable_t *hashtable, const char *key, void *value);

bool hashtable_remove(hashtable_t *hashtable, const char *key);

void *hashtable_find(hashtable_t *hashtable, const char *key);
    
void hashtable_foreach_impl(hashtable_t *hashtable, void *pass, void *callback, bool keys);

hashtable_t *hashtable_copy_impl(hashtable_t *hashtable, void *(*copy)(void *));

size_t hashtable_elements(hashtable_t *hashtable);

#define hashtable_foreach_3(HASHTABLE, PASS, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), (void *)(PASS), (void *)(CALLBACK), false)
#define hashtable_foreach_2(HASHTABLE, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), NULL, (void *)(CALLBACK), false)

#define hashtable_foreachkv_3(HASHTABLE, PASS, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), (void *)(PASS), (void *)(CALLBACK), true)
#define hashtable_foreachkv_2(HASHTABLE, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), NULL, (void *)(CALLBACK), true)

#define hashtable_foreach(...) \
    DEFAULT(hashtable_foreach, __VA_ARGS__)
#define hashtable_foreachkv(...) \
    DEFAULT(hashtable_foreachkv, __VA_ARGS__)

#define hashtable_copy(HASHTABLE, COPY) \
    hashtable_copy_impl((HASHTABLE), (void *(*)(void *))(COPY))

#endif /*!REDROID_HASHTABLE_HDR */
