#ifndef DEVICE_IO_IMPL_H
#define DEVICE_IO_IMPL_H

#include "device_io.h"

struct device_io *create_device_io_impl(int, int);
void free_io_impl(struct device_io *);

#endif