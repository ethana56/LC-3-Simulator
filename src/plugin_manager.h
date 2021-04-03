#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <stdio.h>

#include "list.h"

#define PM_ERROR_STR_SIZ BUFSIZ

struct device_data {
    void *dlhandle;
    struct device *device;
};

List *pm_load_device_plugins(const char *, const char *, char *);
void pm_plugins_free(List *);

#endif
