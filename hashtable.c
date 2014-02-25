#include "hashtable.h"
#include "list.h"

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

struct hashtable_s {
    size_t          size;
    pthread_mutex_t mutex;
    list_t        **table;
    bool          (*compare)(const void *, const size_t, const void *);
    size_t        (*hash)(const void *, const size_t);
};

typedef struct {
    const void  *key;
    void        *value;
    hashtable_t *hashtable;
    size_t       keylength;
} hashtable_entry_t;

static inline hashtable_entry_t *hashtable_entry_create(const void *key, const size_t keylength, void *value) {
    hashtable_entry_t *entry = malloc(sizeof(*entry));

    entry->key       = key;
    entry->value     = value;
    entry->keylength = keylength;

    return entry;
}

static inline void hashtable_entry_destroy(hashtable_entry_t *entry) {
    free(entry);
}

static inline bool hashtable_entry_compare(const void *a, const void *b) {
    const hashtable_entry_t *const entry = a;
    const hashtable_entry_t *const other = b;

    if (entry->keylength != other->keylength)
        return false;

    return other->hashtable->compare(entry->key, other->keylength, other->key);
}

static inline void *hashtable_entry_find(hashtable_t *hashtable, const void *key, const size_t keylength, size_t *index) {
    *index = hashtable->hash(key, keylength) & (hashtable->size - 1);

    hashtable_entry_t pass = {
        .key       = key,
        .keylength = keylength,
        .hashtable = hashtable
    };

    return list_search(hashtable->table[*index], &hashtable_entry_compare, &pass);
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

static bool hashtable_funcs_compare(const void *a, const size_t length, const void *b) {
    return !memcmp(a, b, length);
}

static size_t hashtable_funcs_hash(const void *data, const size_t length) {
    size_t hash = 5381;
    size_t value;

    /* read data as size_t */
    union {
        const size_t *s;
        const void   *v;
    } cast = {
        .v = data
    };

    for (size_t i = 0; i < length / sizeof(size_t); i++) {
        value = cast.s[i];
        hash  = ((hash << 5) + hash) + value;
    }

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
    hashtable->compare = hashtable_funcs_compare;
    hashtable->hash    = hashtable_funcs_hash;

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

void hashtable_insert(hashtable_t *hashtable, const void *key, const size_t keylength, void *value) {
    size_t hash = hashtable->hash(key, keylength) & (hashtable->size - 1);
    pthread_mutex_lock(&hashtable->mutex);
    list_push(hashtable->table[hash], hashtable_entry_create(key, keylength, value));
    pthread_mutex_unlock(&hashtable->mutex);
}

bool hashtable_remove(hashtable_t *hashtable, const void *key, const size_t keylength) {
    bool   value = true;
    size_t index = 0;

    pthread_mutex_lock(&hashtable->mutex);
    hashtable_entry_t *find = hashtable_entry_find(hashtable, key, keylength, &index);
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

void *hashtable_find(hashtable_t *hashtable, const void *key, const size_t keylength) {
    pthread_mutex_lock(&hashtable->mutex);
    hashtable_entry_t *find = hashtable_entry_find(hashtable, key, keylength, &(size_t){0});
    pthread_mutex_unlock(&hashtable->mutex);

    return find ? find->value : NULL;
}

void hashtable_foreach(hashtable_t *hashtable, void (*callback)(void *)) {
    pthread_mutex_lock(&hashtable->mutex);
    for (size_t i = 0; i < hashtable->size; i++) {
        list_t *list = hashtable->table[i];
        if (list_length(list) == 0)
            break;

        list_iterator_t *it = list_iterator_create(list);
        while (!list_iterator_end(it)) {
            hashtable_entry_t *entry = list_iterator_next(it);
            callback(entry->value);
        }
        list_iterator_destroy(it);
    }
    pthread_mutex_unlock(&hashtable->mutex);
}

void hashtable_set_compare(hashtable_t *hashtable, bool (*compare)(const void *, const size_t, const void *)) {
    hashtable->compare = (compare) ? compare: &hashtable_funcs_compare;
}

void hashtable_set_hash(hashtable_t *hashtable, size_t (*hash)(const void *, const size_t)) {
    hashtable->hash = (hash) ? hash : &hashtable_funcs_hash;
}