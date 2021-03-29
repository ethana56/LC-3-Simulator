#include <stdlib.h>
#include <string.h>

#include "list.h"

struct list {
    void *data;
    size_t data_size;
    size_t num_elements;
    size_t capacity;
    double expand_mult;
};

List *list_new(size_t data_size, size_t init_capacity, double expand_mult) {
    List *list = malloc(sizeof(struct list));
    if (list == NULL) {
        return NULL;
    }
    list->data = malloc(data_size * init_capacity);
    if (list->data == NULL) {
        free(list);
        return NULL;
    }
    list->data_size = data_size;
    list->num_elements = 0;
    list->capacity = init_capacity;
    list->expand_mult = expand_mult;
    return list;
}

static int list_resize(List *list) {
    void *new_data;
    size_t new_capacity;
    new_capacity = list->capacity * list->expand_mult;
    new_data = realloc(list->data, new_capacity * list->data_size);
    if (new_data == NULL) {
        return -1;
    }
    list->data = new_data;
    list->capacity = new_capacity;
    return 0;
}

int list_add(List *list, void *data) {
    if (list->num_elements == list->capacity) {
        if (list_resize(list) < 0) {
            return -1;
        }
    }
    memcpy((char *)list->data + (list->num_elements * list->data_size), data, list->data_size);
    ++list->num_elements;
    return 0;
}

void *list_to_array_no_cpy(List *list, size_t *num_elements) {
    *num_elements = list->num_elements;
    return list->capacity;
}

void *convert_to_array(List *list, size_t *num_elements) {
    void *array;
    *num_elements = list->num_elements;
    if (list->num_elements == 0) {
        free(list->data);
        free(list);
        return NULL;
    }
    array = realloc(list->data, list->data_size * list->num_elements);
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