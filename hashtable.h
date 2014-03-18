#ifndef REDROID_HASHTABLE_HDR
#define REDROID_HASHTABLE_HDR
#include <stdbool.h>
#include <stddef.h>

/*
 * Type: hashtable_t
 *  Type capable of representing a hashtable.
 *
 * Remarks:
 *  This implementation of a hashtable is a standard seperate-chaining
 *  hashtable with the used of linked-lists. The implementation itself
 *  uses mutual exclusion for thread-safety as well.
 */
typedef struct hashtable_s hashtable_t;

/*
 * Function: hashtable_create
 *  Create a hashtable
 *
 * Parameters:
 *  size    - The size of buckets for the hashtable.
 *
 * Returns:
 *  A new hashtable or NULL on failure.
 */
hashtable_t *hashtable_create(size_t size);

/*
 * Function: hashtable_destroy
 *  Destroy a hashtable
 *
 * Parameters:
 *  hashtable   - The hashtable to destroy
 */
void hashtable_destroy(hashtable_t *hashtable);

/*
 * Function: hashtable_insert
 *  Insert value referenced by pointer in hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to insert into.
 *  key         - The key.
 *  value       - Value referenced by pointer to store in hashtable.
 */
void hashtable_insert(hashtable_t *hashtable, const char *key, void *value);

/*
 * Function: hashtable_remove
 *  Remove value referenced by pointer from hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to remove from.
 *  key         - The key.
 *
 * Returns:
 *  If the value referenced by pointer is found and removed from the
 *  hashtable true is returned. On failure false is returned.
 */
bool hashtable_remove(hashtable_t *hashtable, const char *key);

/*
 * Function: hashtable_find
 *  Find value referenced by pointer in hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to find for value referenced by pointer in.
 *  key         - Pointer to key data to find.
 *
 * Returns:
 *  The value referenced by pointer on success. On failure NULL is returned
 *  instead. It should be noted here that it's impossible to distinguish
 *  an error if the value referenced by pointer itself was also NULL.
 */
void *hashtable_find(hashtable_t *hashtable, const char *key);

/*
 * Function: hashtable_foreach
 *  Execute a callback passing in each value in the entire hashtable
 *  as well as passing in an additional pointer.
 *
 * Parameters:
 *  hashtable   - The hashtable to execute the callback over.
 *  pass        - The additional thing to pass in for the callback to
 *                get as its second argument.
 *  callback    - Pointer to function callback.
 */
#define hashtable_foreach(HASHTABLE, PASS, CALLBACK) \
    hashtable_foreach_impl((HASHTABLE), (PASS), ((void(*)(void*,void*))(CALLBACK)))

void hashtable_foreach_impl(hashtable_t *hashtable, void *pass, void (*callback)(void *, void *));

#endif /*!REDROID_HASHTABLE_HDR */
