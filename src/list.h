#ifndef LIST_H
#define LIST_H


struct list;
typedef struct list List;

List *new_List(size_t, size_t, double);
int list_add(List *, void *);
void *list_to_array_no_cpy(List *, size_t *);
void *convert_to_array(List *, size_t *);
void list_free(List *);

#endif
