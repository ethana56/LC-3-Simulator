#include <stdlib.h>
#include <string.h>

#include "list.h"

struct list {
    void *data;
    struct list_allocator *allocator;
    size_t data_size;
    size_t num_elements;
    size_t capacity;
    double expand_mult;
};

List *list_new(size_t data_size, size_t init_capacity, double expand_mult, struct list_allocator *allocator) {
    List *list = allocator->alloc(sizeof(struct list));
    if (list == NULL) {
        return NULL;
    }
    list->data = allocator->alloc(data_size * init_capacity);
    if (list->data == NULL) {
        free(list);
        return NULL;
    }
    list->data_size = data_size;
    list->num_elements = 0;
    list->capacity = init_capacity;
    list->expand_mult = expand_mult;
    list->allocator = allocator;
    return list;
}

static int list_resize(List *list) {
    void *new_data;
    size_t new_capacity;
    new_capacity = list->capacity * list->expand_mult;
    new_data = list->allocator->realloc(list->data, new_capacity * list->data_size);
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

void list_clear(List *list) {
    list->num_elements = 0;
}

void *list_get(List *list, size_t index) {
    if (index >= list->num_elements) {
        return NULL;
    }
    return (char *)list->data + (list->data_size * index);
}

void list_sort(List *list, int (*comparator)(const void *, const void *)) {
    qsort(list->data, list->num_elements, list->data_size, comparator);
}

void *list_bsearch(List *list, void const * key, int (*comparator)(const void *, const void *)) {
    return bsearch(key, list->data, list->num_elements, list->data_size, comparator);
}

void *list_get_array(List *list) {
    return list->data;
}

size_t list_num_elements(List *list) {
    return list->num_elements;
}

void list_free(List *list) {
    struct list_allocator *allocator;
    if (list == NULL) {
        return;
    }
    allocator = list->allocator;
    allocator->free(list->data);
    allocator->free(list);
}

void list_remove(List *list, size_t index) {
    size_t cur_move_index;
    if (index >= list->num_elements) {
        return;
    }
    for (cur_move_index = index + 1; cur_move_index < list->num_elements; ++cur_move_index) {
        void *dst, *src;
        dst = (char *)list->data + (list->data_size * (cur_move_index - 1));
        src = (char *)list->data + (list->data_size * cur_move_index);
        memcpy(dst, src, list->data_size);
    }
    --list->num_elements;
}

void *list_convert_to_array(List *list, size_t *num_elements) {
    void *array;
    *num_elements = list->num_elements;
    if (list->num_elements == 0) {
        list_free(list);
        return NULL;
    }
    array = list->allocator->realloc(list->data, list->data_size * list->num_elements);
    if (array == NULL) {
        array = list->data;
    }
    list_free(list);
    return array;
}

void list_compact(List *list) {

}