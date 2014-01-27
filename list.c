#include "list.h"

#include <stdlib.h>

typedef struct list_node_s list_node_t;

struct list_node_s {
    void        *element;
    list_node_t *next;
    list_node_t *prev;
};

struct list_s {
    size_t       length;
    list_node_t *head;
    list_node_t *tail;
};

struct list_iterator_s {
    list_t      *list;
    list_node_t *pointer;
};

// iterator
list_iterator_t *list_iterator_create(list_t *list) {
    list_iterator_t *it = malloc(sizeof(*it));

    it->pointer = list->head;
    it->list    = list;

    return it;
}

void list_iterator_destroy(list_iterator_t *it) {
    free(it);
}

static void *list_iterator_walk(list_iterator_t *it, size_t offset) {
    if (!it->pointer) return NULL;
    void *ret = it->pointer->element;
    it->pointer = *(void **)(((unsigned char *)it->pointer) + offset);
    return ret;
}

void *list_iterator_next(list_iterator_t *it) {
    return list_iterator_walk(it, offsetof(list_node_t, next));
}

void *list_iterator_prev(list_iterator_t *it) {
    return list_iterator_walk(it, offsetof(list_node_t, prev));
}

void list_iterator_reset(list_iterator_t *it) {
    it->pointer = it->list->head;
}

bool list_iterator_end(list_iterator_t *it) {
    return !it->pointer;
}

// list node
static list_node_t *list_node_create(void *element) {
    list_node_t *node = malloc(sizeof(*node));

    node->element = element;
    node->next    = NULL;
    node->prev    = NULL;

    return node;
}

static void list_node_destroy(list_node_t *node) {
    free(node);
}

static void list_node_scrub(list_node_t **node) {
    list_node_destroy(*node);
    *node = NULL;
}

// list
list_t *list_create(void) {
    list_t *list = malloc(sizeof(*list));
    list->length = 0;
    list->head   = NULL;
    list->tail   = NULL;

    return list;
}

void list_destroy(list_t *list) {
    list_node_t *temp;
    for (list_node_t *node = list->head; node; ) {
        temp = node->next;
        list_node_destroy(node);
        node = temp;
    }
    free(list);
}

void list_push(list_t *list, void *element) {
    list_node_t *node = list_node_create(element);
    if (!list->head)
        list->head = node;
    else {
        list->tail->next = node;
        node->prev       = list->tail;
    }
    list->tail = node;
    list->length++;
}

void *list_pop(list_t *list) {
    if (!list->head)
        return NULL;

    void *element = list->tail->element;
    list->tail = list->tail->prev;
    list_node_scrub((list->tail) ? &list->tail->next : &list->head);
    list->length--;
    return element;
}

void *list_shift(list_t *list) {
    if (!list->head)
        return NULL;

    void *element = list->head->element;
    list->head = list->head->next;
    list_node_scrub((list->head) ? &list->head->prev : &list->tail);
    list->length--;
    return element;
}

bool list_erase(list_t *list, void *element) {
    for (list_node_t *curr = list->head; curr; curr = curr->next) {
        if (curr->element != element)
            continue;

        if (curr == list->head)
            list->head = list->head->next;
        if (curr == list->tail)
            list->tail = list->tail->prev;
        if (curr->next)
            curr->next->prev = curr->prev;
        if (curr->prev)
            curr->prev->next = curr->next;

        list_node_destroy(curr);
        list->length--;
        return true;
    }
    return false;
}

size_t list_length(list_t *list) {
    return (list) ? list->length : 0;
}

static list_node_t *list_sort_split(list_node_t *node) {
    if (!node || !node->next) return NULL;
    list_node_t *split = node->next;
    node->next  = split->next;
    split->next = list_sort_split(split->next);
    return split;
}

static list_node_t *list_sort_merge(list_node_t *a, list_node_t *b, bool (*predicate)(const void *, const void *)) {
    if (!a) return b;
    if (!b) return a;
    if (predicate(a->element, b->element)) {
        b->next       = list_sort_merge(a, b->next, predicate);
        b->next->prev = b;
        return b;
    }
    a->next       = list_sort_merge(a->next, b, predicate);
    a->next->prev = a;
    return a;
}

static list_node_t *list_sort_impl(list_node_t *begin, bool (*predicate)(const void *, const void *)) {
    if (!begin)       return NULL;
    if (!begin->next) return begin;

    list_node_t *split = list_sort_split(begin);
    return list_sort_merge(list_sort_impl(begin, predicate), list_sort_impl(split, predicate), predicate);
}

void list_sort(list_t *list, bool (*predicate)(const void *, const void *)) {
    list->head = list_sort_impl(list->head, predicate);
}
