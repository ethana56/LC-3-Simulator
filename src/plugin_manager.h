#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <stdio.h>

#include "list.h"

#define PM_ERROR_STR_SIZ BUFSIZ

typedef struct plugin_manager PluginManager;
typedef struct plugin_manager_iterator PluginManagerIterator;

enum pm_error {PM_ERROR_PLUGIN_LOAD, PM_ERROR_OPENDIR};

struct pm_device_data {
    const char *name;
    const char *path;
    struct device *device;
};

PluginManager *pm_new(void (*on_error)(const char *, const char *, enum pm_error, void *), void *);
void pm_free(PluginManager *);
void pm_load_device_plugins(PluginManager *, List *dir_paths, const char *extension);
PluginManagerIterator *pm_get_iterator(PluginManager *);
struct pm_device_data *pm_iterator_next(PluginManagerIterator *, struct pm_device_data *device_data);
void pm_iterator_free(PluginManagerIterator *);

void pm_set_on_error(void (*)(const char *, const char *, enum pm_error, void *), void *);

#endif
