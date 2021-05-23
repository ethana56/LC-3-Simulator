#ifndef PM_NAME_MANAGER_H
#define PM_NAME_MANAGER_H

typedef struct pm_name_manager PMNameManager;
typedef struct pm_name_manager_iterator PMNameManagerIterator;

struct plugin_names {
    char *name;
    char *path;
};

PMNameManager *pmnm_new(void);
void pmnm_add_path(PMNameManager *, const char *);
PMNameManagerIterator *pmnm_to_iterator(PMNameManager *);
void pmnm_iterator_free(PMNameManagerIterator *);
struct plugin_names *pmnm_iterator_next(PMNameManagerIterator *);

#endif