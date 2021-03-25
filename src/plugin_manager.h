#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <stdio.h>

#include "device_plugin.h"

#define PM_ERROR_STR_SIZ BUFSIZ

struct device_plugin_data {
    void *dlhandle;
    struct device_plugin *plugin;
};

struct device_plugin_data **pm_load_device_plugins(const char *, const char *, size_t *, struct host *, char *);
void pm_plugins_free(struct device_plugin_data **plugins, size_t num_plugins);

#endif
