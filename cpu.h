#ifndef CPU_H
#define CPU_H

#include "io.h"

struct Cpu;
typedef struct Cpu Cpu;

Cpu *new_Cpu(char *);
void cpu_execute_until_end(Cpu *);
int cpu_attach_devices(Cpu *, struct io_device **, int);

enum reg {r0, r1, r2, r3, r4, r5, r6, r7, num_registers};
enum pl {PL7, PL6, PL5, PL4, PL3, PL2, PL1, PL0};

#endif
