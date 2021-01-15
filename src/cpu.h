#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define IO_REGISTERS_MAX 512
#define MAX_PROGRAM_LEN 56124

struct Cpu;
typedef struct Cpu Cpu;

struct device {
    void *data;
    uint16_t *readable;
    uint16_t *writeable;
    uint16_t *readable_writeable;
    size_t num_readable;
    size_t num_writeable;
    size_t num_readable_writeable;
    uint16_t (*read_register)(struct device *, uint16_t);
    void (*write_register)(struct device *, uint16_t, uint16_t);
};

Cpu *new_Cpu();
uint16_t cpu_read_memory(Cpu *, uint16_t);
void cpu_write_memory(Cpu *, uint16_t, uint16_t);
void cpu_load_program(Cpu *, const uint16_t *, size_t);
void cpu_execute_until_end(Cpu *);
int cpu_attach_devices(Cpu *, struct device **, size_t);
void add_interrupt_controller(Cpu *, void (*interrupt_controller)(uint8_t *, uint8_t));
void free_cpu(Cpu *);

enum reg {r0, r1, r2, r3, r4, r5, r6, r7, num_registers};
enum pl {PL7, PL6, PL5, PL4, PL3, PL2, PL1, PL0};

#endif
