#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define IO_REGISTERS_MAX 512
#define MAX_PROGRAM_LEN 56124

#define INTERRUPT_VEC_SIZE UINT8_MAX

struct cpu;
typedef struct cpu Cpu;

struct interrupt_checker {
    void *data;
    int (*check_interrupt)(struct interrupt_checker *, uint8_t cmp_priority, uint8_t *, uint8_t *, int (*)(uint8_t, uint8_t));
};

struct bus_accessor {
    void *data;
    uint16_t (*read)(struct bus_accessor *, uint16_t);
    void (*write)(struct bus_accessor *, uint16_t, uint16_t);
};


Cpu *new_Cpu();
void cpu_execute_until_end(Cpu *);
void cpu_subscribe_on_tick(Cpu *, void (*)(void *), void *);
void cpu_set_program_counter(Cpu *, uint16_t);
void free_cpu(Cpu *);

enum reg {r0, r1, r2, r3, r4, r5, r6, r7, num_registers};

#endif
