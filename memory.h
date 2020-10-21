#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "io.h"

#define USER_ADDRESSES 52736

struct Memory;
typedef struct Memory Memory;

uint16_t read_memory(Memory *, uint16_t);
void write_memory(Memory *, uint16_t, uint16_t);
uint16_t load_program(Memory *, FILE *, int *);
void memory_register_io_registers(Memory *, struct io_register **, int);
Memory *new_Memory(void);

#endif
