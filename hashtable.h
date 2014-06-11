#ifndef REDROID_HASHTABLE_HDR
#define REDROID_HASHTABLE_HDR
#include <stdbool.h>
#include <stddef.h>

typedef struct hashtable_s hashtable_t;

hashtable_t *hashtable_create(size_t size);

void hashtable_destroy(hashtable_t *hashtable);

void hashtable_insert(hashtable_t *hashtable, const char *key, void *value);

bool hashtable_remove(hashtable_t *hashtable, const char *key);

void *hashtable_find(hashtable_t *hashtable, const char *key);

#define hashtable_foreach(HASHTABLE, PASS, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), (PASS), ((void *)(CALLBACK)), false)

#define hashtable_foreachkv(HASHTABLE, PASS, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), (PASS), ((void *)(CALLBACK)), true)

void hashtable_foreach_impl(hashtable_t *hashtable, void *pass, void *callback, bool keys);

#define hashtable_copy(HASHTABLE, COPY) \
    hashtable_copy_impl((HASHTABLE), ((void *(*)(void *))(COPY)))

hashtable_t *hashtable_copy_impl(hashtable_t *hashtable, void *(*copy)(void *));

size_t hashtable_elements(hashtable_t *hashtable);

#endif /*!REDROID_HASHTABLE_HDR */
