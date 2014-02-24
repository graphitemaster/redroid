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
 *  key         - Pointer to key data.
 *  keylength   - Length of key data.
 *  value       - Value referenced by pointer to store in hashtable.
 */
void hashtable_insert(hashtable_t *hashtable, const void *key, const size_t keylength, void *value);

/*
 * Function: hashtable_remove
 *  Remove value referenced by pointer from hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to remove from.
 *  key         - Pointer to key data to remove.
 *  keylength   - Length of key data.
 *
 * Returns:
 *  If the value referenced by pointer is found and removed from the
 *  hashtable true is returned. On failure false is returned.
 */
bool hashtable_remove(hashtable_t *hashtable, const void *key, const size_t keylength);

/*
 * Function: hashtable_find
 *  Find value referenced by pointer in hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to find for value referenced by pointer in.
 *  key         - Pointer to key data to find.
 *  keylength   - Length of key data.
 *
 * Returns:
 *  The value referenced by pointer on success. On failure NULL is returned
 *  instead. It should be noted here that it's impossible to distinguish
 *  an error if the value referenced by pointer itself was also NULL.
 */
void *hashtable_find(hashtable_t *hashtable, const void *key, const size_t keylength);

/*
 * Function: hashtable_foreach
 *  Execute a callback passing in each value in the entire hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to execute callback over.
 *  callback    - Pointer to function callback
 */
void hashtable_foreach(hashtable_t *hashtable, void (*callback)(void *));

/*
 * Function: hashtable_set_compare
 *  Set the comparision function for the hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to set the comparision function of.
 *  compare     - Pointer to the comparision function.
 *
 * Remarks:
 *  If `compare` is NULL then the default compare function provided by
 *  the hashtable implementation will be set.
 */
void hashtable_set_compare(hashtable_t *hashtable, bool (*compare)(const void *, const size_t, const void *));

/*
 * Function: hashtable_set_hash
 *  Set the hash function for the hashtable.
 *
 * Parameters:
 *  hashtable   - The hashtable to set the hash function of.
 *  hash        - Pointer to the hash function.
 *
 * Remarks:
 *  If `hash` is NULL then the default hash function provided by the
 *  hashtable implementation will be set.
 */
void hashtable_set_hash(hashtable_t *hashtable, size_t (*hash)(const void *, const size_t));

#endif /*!REDROID_HASHTABLE_HDR */
