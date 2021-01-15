#ifndef DEVICE_IMPL_H
#define DEVICE_IMPL_H

#include "cpu.h"
#include "device_plugin.h"

void init_device_impl(int, int, void (*)(char *));
struct device *create_device_impl(struct device_plugin *);
struct device_plugin *device_impl_free(struct device *);


#endif
