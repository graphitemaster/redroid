#ifndef REDROID_LIST_HDR
#define REDROID_LIST_HDR
#include <stdbool.h>
#include <stddef.h>

typedef struct list_iterator_s list_iterator_t;
typedef struct list_s          list_t;

list_iterator_t *list_iterator_create(list_t *list);
void list_iterator_destroy(list_iterator_t *iterator);
void list_iterator_reset(list_iterator_t *iterator);
bool list_iterator_end(list_iterator_t *iterator);
void *list_iterator_next(list_iterator_t *iterator);
void *list_iterator_prev(list_iterator_t *iterator);

list_t *list_create(void);
void list_destroy(list_t *list);
void list_push(list_t *list, void *element);
void *list_pop(list_t *list);
void *list_shift(list_t *list);
list_t *list_copy(list_t *list);
size_t list_length(list_t *list);
bool list_erase(list_t *list, void *element);
void list_sort(list_t *list, bool (*predicate)(const void *, const void *));

#endif
