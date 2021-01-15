#ifndef LIST_H
#define LIST_H


struct list;
typedef struct list List;

List *new_List(size_t, double);
int list_add(List *, void *);
int list_add_all(List *, void **, size_t);
void *list_get(List *, size_t);
size_t list_size(List *);
void **list_free_and_return_as_array(List *, size_t *);
void list_free(List *);

#endif
