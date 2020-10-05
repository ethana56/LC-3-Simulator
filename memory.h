#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "io.h"

#define USER_ADDRESSES 52736

uint16_t read_memory(uint16_t);
void write_memory(uint16_t, uint16_t);
uint16_t load_program(FILE *, int *);
void memory_register_io_registers(struct io_register **, int);

#endif
