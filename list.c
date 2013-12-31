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

void *list_iterator_next(list_iterator_t *it) {
    void *ret;

    if (!it->pointer)
        return NULL;

    ret         = it->pointer->element;
    it->pointer = it->pointer->next;

    return ret;
}

void list_iterator_reset(list_iterator_t *it) {
    it->pointer = it->list->head;
}

bool list_iterator_end(list_iterator_t *it) {
    return !it->pointer;
}

// list node
list_node_t *list_node_create(void *element) {
    list_node_t *node = malloc(sizeof(*node));

    node->element = element;
    node->next    = NULL;
    node->prev    = NULL;

    return node;
}

void list_node_destroy(list_node_t *node) {
    free(node);
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
    if (list->tail) {
        list_node_destroy(list->tail->next);
        list->tail->next = NULL;
    } else {
        list_node_destroy(list->head);
        list->head = NULL;
    }
    list->length--;
    return element;
}

bool list_erase(list_t *list, void *element) {
    for (list_node_t *curr = list->head; curr; curr = curr->next) {
        if (curr->element != element)
            continue;
        *((curr->prev) ? &curr->prev->next : &list->head) = curr->next;
        *((curr->next) ? &curr->next->prev : &list->tail) = curr->prev;
        list_node_destroy(curr);
        return true;
    }
    return false;
}

size_t list_length(list_t *list) {
    return (list) ? list->length : 0;
}
