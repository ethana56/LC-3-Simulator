#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define IO_REGISTERS_MAX 512
#define MAX_PROGRAM_LEN 56124

#define INTERRUPT_VEC_SIZE UINT8_MAX

struct cpu;
typedef struct cpu Cpu;

enum lc3_reg {REG_R0, REG_R1, REG_R2, REG_R3, REG_R4, REG_R5, 
    REG_R6, REG_R7, num_registers};

struct bus_accessor {
    void *data;
    uint16_t (*read)(struct bus_accessor *, uint16_t);
    void (*write)(struct bus_accessor *, uint16_t, uint16_t);
};

Cpu *new_Cpu(struct bus_accessor *);
int cpu_tick(Cpu *);
int cpu_signal_interrupt(Cpu *, uint8_t, uint8_t);
void cpu_set_program_counter(Cpu *, uint16_t);
void free_cpu(Cpu *);

#endif
