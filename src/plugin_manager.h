#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "device_plugin.h"

#define PM_ERROR_STR_SIZ BUFSIZ

void pm_free_plugin(struct device_plugin *);
struct device_plugin **pm_load_device_plugins(const char *, size_t *, char *);

#endif
