#include <stdlib.h>
#include <stdio.h> /*TEMPORARY */
#include "list.h"

struct list {
    void **data;
    size_t size;
    size_t capacity;
    double expand_mult;
};

List *new_List(size_t init_capacity, double expand_mult) {
    List *list = malloc(sizeof(struct list));
    if (list == NULL) {
        return NULL;
    }
    list->data = malloc(sizeof(void *) * init_capacity);
    if (list->data == NULL) {
        free(list);
        return NULL;
    }
    list->capacity = init_capacity;
    list->size = 0;
    list->expand_mult = expand_mult;
    return list;
}

int list_add(List *list, void *data) {
    size_t old_capacity;
    void *new_data;
    if (list->size == list->capacity) {
        old_capacity = list->capacity;
        list->capacity += (list->capacity * list->expand_mult);
        new_data = realloc(list->data, sizeof(void *) * list->size);
        if (new_data == NULL) {
            list->capacity = old_capacity;
            return -1;
        }
        list->data = new_data;
    }
    list->data[list->size++] = data;
    return 0;
}

int list_add_all(List *list, void **data, size_t amt) {
    size_t i;
    for (i = 0; i < amt; ++i) {
        if (list_add(list, data[i]) < 0) return -1;
    }
    return 0;
}

void *list_to_array_no_copy(List *list) {
    return list->data;
}

size_t list_size(List *list) {
    return list->size;
}

void *list_get(List *list, size_t index) {
    if (index >= list->size || index < 0) {
        return NULL;
    }
    return list->data[index];
}

void **list_free_and_return_as_array(List *list, size_t *num_elements) {
    void **array;
    if (list == NULL) {
        return NULL;
    }
    *num_elements = list->size;
    if (list->size == 0) {
        free(list->data);
        free(list);
        return NULL;
    }
    array = realloc(list->data, sizeof(void *) * list->size);
    if (array == NULL) {
        array = list->data;
    }
    free(list);
    return array;
}

void list_free(List *list) {
    if (list == NULL) {
        return;
    }
    free(list->data);
    free(list);
}
