#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#include "lc3_reg.h"

#define IO_REGISTERS_MAX 512
#define MAX_PROGRAM_LEN 56124

#define INTERRUPT_VEC_SIZE UINT8_MAX

struct cpu;
typedef struct cpu Cpu;

struct bus_accessor {
    void *data;
    uint16_t (*read)(struct bus_accessor *, uint16_t);
    void (*write)(struct bus_accessor *, uint16_t, uint16_t);
};

Cpu *new_Cpu(struct bus_accessor *);
int cpu_tick(Cpu *);
int cpu_signal_interrupt(Cpu *, uint8_t, uint8_t);
uint16_t cpu_read_register(Cpu *, enum lc3_reg);
void cpu_write_register(Cpu *, enum lc3_reg, uint16_t);
void free_cpu(Cpu *);

#endif
