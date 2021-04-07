#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

struct list;
typedef struct list List;

List *list_new(size_t, size_t, double);
int list_add(List *, void *);
void list_clear(List *);
void *list_get(List *, size_t);
void list_sort(List *, int (*)(const void *, const void *));
void *list_bsearch(List *, void const *, int (*)(const void *, const void *));
size_t list_num_elements(List *);
void *list_get_array(List *);
void *list_convert_to_array(List *, size_t *);
void list_free(List *);

#endif
