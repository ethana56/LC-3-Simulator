#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <stdio.h>

#include "list.h"

#define PM_ERROR_STR_SIZ BUFSIZ

enum pm_error {PM_ERROR_PLUGIN_LOAD, PM_ERROR_OPENDIR};

struct device_data {
    void *dlhandle;
    char *name;
    char *path;
    struct device *device;
};

void pm_set_on_error(void (*)(const char *, const char *, enum pm_error, void *), void *);
void pm_free_plugin(struct device_data *);
List *pm_load_device_plugins(const char **, size_t, const char *);
void pm_plugins_free(List *);

#endif
