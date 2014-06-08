#include "hashtable.h"
#include "list.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct hashtable_s {
    size_t          size;
    pthread_mutex_t mutex;
    list_t        **table;
    bool          (*compare)(const char *, const char *);
    size_t        (*hash)(const char *);
};

typedef struct {
    char        *key;
    void        *value;
    hashtable_t *hashtable;
} hashtable_entry_t;

static inline hashtable_entry_t *hashtable_entry_create(const void *key, void *value) {
    hashtable_entry_t *entry = malloc(sizeof(*entry));

    entry->key       = strdup(key);
    entry->value     = value;

    return entry;
}

static inline void hashtable_entry_destroy(hashtable_entry_t *entry) {
    free(entry->key);
    free(entry);
}

static inline void *hashtable_entry_find(hashtable_t *hashtable, const char *key, size_t *index) {
    *index = hashtable->hash(key) & (hashtable->size - 1);

    hashtable_entry_t pass = {
        .key       = (char *)key,
        .hashtable = hashtable
    };

    return list_search(hashtable->table[*index], &pass,
        lambda bool(const hashtable_entry_t *entrya, const hashtable_entry_t *entryb) {
            return entryb->hashtable->compare(entrya->key, entryb->key);
        }
    );
}

static inline size_t hashtable_pot(size_t size) {
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 8;
    size |= size >> 16;

    /* for 64 bit size_t */
    if (sizeof(size_t) == 8)
        size |= size >> 32;

    size++;
    return size;
}

static bool hashtable_funcs_compare(const char *a, const char *b) {
    return !strcmp(a, b);
}

static size_t hashtable_funcs_hash(const char *string) {
    size_t hash   = 0;
    int    ch     = 0;

    while ((ch = *string++))
        hash = ch + (hash << 6) + (hash << 16) - hash;

    return hash;
}

hashtable_t *hashtable_create(size_t size) {
    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL) != 0)
        return NULL;

    hashtable_t *hashtable = malloc(sizeof(*hashtable));

    hashtable->size    = hashtable_pot(size);
    hashtable->mutex   = mutex;
    hashtable->table   = malloc(sizeof(list_t*) * size);
    hashtable->hash    = &hashtable_funcs_hash;
    hashtable->compare = &hashtable_funcs_compare;

    for (size_t i = 0; i < hashtable->size; i++)
        hashtable->table[i] = list_create();

    return hashtable;
}

void hashtable_destroy(hashtable_t *hashtable) {
    pthread_mutex_lock(&hashtable->mutex);
    for (size_t i = 0; i < hashtable->size; i++) {
        hashtable_entry_t *entry;
        while ((entry = list_pop(hashtable->table[i])))
            hashtable_entry_destroy(entry);
        list_destroy(hashtable->table[i]);
    }

    free(hashtable->table);
    pthread_mutex_unlock(&hashtable->mutex);
    pthread_mutex_destroy(&hashtable->mutex);
    free(hashtable);
}

void hashtable_insert(hashtable_t *hashtable, const char *key, void *value) {
    size_t hash = hashtable->hash(key) & (hashtable->size - 1);
    pthread_mutex_lock(&hashtable->mutex);
    list_push(hashtable->table[hash], hashtable_entry_create(key, value));
    pthread_mutex_unlock(&hashtable->mutex);
}

bool hashtable_remove(hashtable_t *hashtable, const char *key) {
    bool   value = true;
    size_t index = 0;

    pthread_mutex_lock(&hashtable->mutex);
    hashtable_entry_t *find = hashtable_entry_find(hashtable, key, &index);
    if (!find) {
        value = false;
        goto hashtable_remove_finish;
    }

    if (!list_erase(hashtable->table[index], find)) {
        value = false;
        goto hashtable_remove_finish;
    }

hashtable_remove_finish:
    pthread_mutex_unlock(&hashtable->mutex);
    return value;
}

void *hashtable_find(hashtable_t *hashtable, const char *key) {
    pthread_mutex_lock(&hashtable->mutex);
    hashtable_entry_t *find = hashtable_entry_find(hashtable, key, &(size_t){0});
    pthread_mutex_unlock(&hashtable->mutex);

    return find ? find->value : NULL;
}

void hashtable_foreach_impl(hashtable_t *hashtable, void *pass, void (*callback)(void *, void *)) {
    pthread_mutex_lock(&hashtable->mutex);
    for (size_t i = 0; i < hashtable->size; i++) {
        list_t *list = hashtable->table[i];
        if (!list)
            break;

        list_iterator_t *it = list_iterator_create(list);
        while (!list_iterator_end(it)) {
            hashtable_entry_t *entry = list_iterator_next(it);
            callback(entry->value, pass);
        }
        list_iterator_destroy(it);
    }
    pthread_mutex_unlock(&hashtable->mutex);
}

hashtable_t *hashtable_copy_impl(hashtable_t *hashtable, void *(*copy)(void *)) {
    hashtable_t *copied = hashtable_create(hashtable->size);
    pthread_mutex_lock(&hashtable->mutex);
    for (size_t i = 0; i < hashtable->size; i++) {
        list_t *list = hashtable->table[i];
        if (!list)
            break;

        list_iterator_t *it = list_iterator_create(list);
        while (!list_iterator_end(it)) {
            hashtable_entry_t *entry = list_iterator_next(it);
            hashtable_insert(copied, entry->key, copy(entry->value));
        }
        list_iterator_destroy(it);
    }
    pthread_mutex_unlock(&hashtable->mutex);
    return copied;
}
