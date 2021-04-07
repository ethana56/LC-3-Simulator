#ifndef BUS_H
#define BUS_H

#include <stdint.h>

#include "device.h"

#define BUS_NUM_ADDRESSES 65536

struct bus_impl;
typedef struct bus_impl Bus;

Bus *bus_new(void);
void bus_free(Bus *);

void bus_print(Bus *);

int bus_attach(Bus *, struct device *);

/*void bus_remove_all_attachments(Bus *bus);*/

int bus_is_device_register(Bus *, uint16_t);
uint16_t bus_read_memory(Bus *, uint16_t);

uint16_t bus_read(Bus *, uint16_t);
void bus_write(Bus *, uint16_t, uint16_t);

#endif