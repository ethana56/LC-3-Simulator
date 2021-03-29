#ifndef BUS_H
#define BUS_H

#include <stdint.h>

#define BUS_NUM_ADDRESSES 65536

enum bus_error {NO_ERROR, READ_ERRNO, ADDRESS_TAKEN};

struct bus_impl;
typedef struct bus_impl Bus;

Bus *bus_new(void);
void bus_free(Bus *);

void bus_print(Bus *);

int bus_attach(Bus *bus, uint16_t (*read)(uint16_t), void (*write)(uint16_t, uint16_t), uint16_t, uint16_t);

/*void bus_remove_all_attachments(Bus *bus);*/

uint16_t bus_read(Bus *, uint16_t);
void bus_write(Bus *, uint16_t, uint16_t);

#endif