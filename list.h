#ifndef LIST_H
#define LIST_H


struct list;
typedef struct list List;

List *new_List(size_t, double);
int list_add(List *, void *);
void *list_get(List *, size_t);
size_t list_size(List *);
void list_free(List *);

#endif
