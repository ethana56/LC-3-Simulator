#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>

#include "cpu.h"
#include "lc3_reg.h"

#define ADD      0x1
#define AND      0x5
#define BR       0x0
#define JMP_RET  0xC
#define JSR_JSRR 0x4
#define LD       0x2
#define LDI      0xA
#define LDR      0x6
#define LEA      0xE
#define NOT      0x9
#define RTI      0x8
#define ST       0x3
#define STI      0xB
#define STR      0x7
#define TRAP     0xF

#define OPCODE(instruction)      ((instruction) >> 12)
#define REG1_INSTRU(instruction) (((instruction) >> 9) & 0x0007)
#define REG2_INSTRU(instruction) (((instruction) >> 6) & 0x0007)
#define REG3_INSTRU(instruction) ((instruction) & 0x0007)
#define IMM5(instruction)        ((instruction) & 0x001F)
#define PCOFFSET9(instruction)   ((instruction) & 0x01FF)
#define PCOFFSET11(instruction)  ((instruction) & 0x07FF)
#define OFFSET6(instruction)     ((instruction) & 0x003F)
#define N_INSTRU(instruction)    ((instruction) & 0x0800)
#define Z_INSTRU(instruction)    ((instruction) & 0x0400)
#define P_INSTRU(instruction)    ((instruction) & 0x0200)
#define NZP_INSTRU(instruction)  ((instruction) >> 9)
#define NZP_PSR(value)           ((value) & 0x0007)
#define TRAPVECT8(instruction)   ((instruction) & 0x00FF)

#define NZP_PSR_CLEAR_MASK       0xFFF8
#define SET_PSR_P_MASK           0x0001
#define SET_PSR_Z_MASK           0x0002
#define SET_PSR_N_MASK           0x0004
#define INIT_PSR_MASK_USER       0x8072
#define INIT_PSR_MASK_SUPERVISOR 0x0002

#define CLOCK_ENABLED(value) (((value) & 0x8000))

#define SUPERVISOR_BIT(psr) ((psr) >> 15)

#define INTERRUPT_VECTOR_TABLE 0x0100
#define SUPERVISOR_STACK_HIGH  0x2FFF

#define IS_IMM5(instruction) (((instruction) >> 5) & 0x0001)
#define IS_JSR(instruction)  (((instruction) >> 11) & 0x0001)

#define PRIORITY_CMP(psr, priority) ((0x0007 & ((psr) >> 8)) > priority)
#define PSR_PRIORITY(psr) (0x0007 & ((psr) >> 8))

#define PRIV_MODE_VIOLATION_EXCEPTION_VECTOR 0x00
#define ILLEGAL_OPCODE_EXCEPTION_VECTOR      0x01

#define MCR_ADDR 0xFFFE

typedef void (*instru_func)(Cpu *, uint16_t);
typedef int (*exception_line)(uint8_t *); 

struct cpu_exception {
   int toggle;
   uint8_t vec_location;
};

struct cpu {
   struct bus_accessor *bus_access;
   struct cpu_exception priv_mode_violation_exception_line;
   struct cpu_exception illegal_opcode_exception_line;
   uint16_t registers[num_registers];
};

static instru_func instru_func_vec[16];
static int instructions_loaded = 0;

static void setup_exceptions(Cpu *cpu) {
   cpu->priv_mode_violation_exception_line.vec_location = PRIV_MODE_VIOLATION_EXCEPTION_VECTOR;
   cpu->priv_mode_violation_exception_line.toggle = 0;
   cpu->illegal_opcode_exception_line.vec_location = ILLEGAL_OPCODE_EXCEPTION_VECTOR;
   cpu->illegal_opcode_exception_line.toggle = 0;
}

static void supervisor_stack_push(Cpu *cpu, uint16_t data) {
   cpu->registers[REG_R6] -= 1;
   cpu->bus_access->write(cpu->bus_access, cpu->registers[REG_R6], data);
}

static uint16_t supervisor_stack_pop(Cpu *cpu) {
   uint16_t data;
   data = cpu->bus_access->read(cpu->bus_access, cpu->registers[REG_R6]);
   cpu->registers[REG_R6] += 1;
   return data;
}

static uint16_t sign_extend(uint16_t value, int num_bits) {
   uint16_t mask = ~(0xFFFF << num_bits);
   uint16_t sign_bit = 1 << (num_bits - 1);
   if (value & sign_bit) {
      return (~((value^mask) + 1)) + 1;
   }
   return value;
}

static void set_condition_code(Cpu *cpu, int reg) {
   int16_t val = cpu->registers[reg];
   cpu->registers[REG_PSR] &= NZP_PSR_CLEAR_MASK;
   if (val > 0) {
      cpu->registers[REG_PSR] |= SET_PSR_P_MASK;
   } else if (val < 0) {
      cpu->registers[REG_PSR] |= SET_PSR_N_MASK;
   } else {
      cpu->registers[REG_PSR] |= SET_PSR_Z_MASK;
   }
}

static void add_and(Cpu *cpu, uint16_t instruction) {
   int dest = REG1_INSTRU(instruction);
   uint16_t sr1_val = cpu->registers[REG2_INSTRU(instruction)];
   uint16_t sr2_val;
   if (IS_IMM5(instruction)) {
      sr2_val = sign_extend(IMM5(instruction), 5);
   } else {
      sr2_val = cpu->registers[REG3_INSTRU(instruction)];
   }
   if (OPCODE(instruction) == ADD) { 
      cpu->registers[dest] = sr1_val + sr2_val;
   } else if (OPCODE(instruction) == AND) {
      cpu->registers[dest] = sr1_val & sr2_val;
   }
   set_condition_code(cpu, dest);
}

static void br(Cpu *cpu, uint16_t instruction) {
   uint16_t pc_offset;
   unsigned nzp_instru = NZP_INSTRU(instruction);
   unsigned nzp_psr    = NZP_PSR(cpu->registers[REG_PSR]);
   if (nzp_instru & nzp_psr) {
      pc_offset = sign_extend(PCOFFSET9(instruction), 9);
      cpu->registers[REG_PC] += pc_offset;
   }
}

static void jmp_ret(Cpu *cpu, uint16_t instruction) {
   cpu->registers[REG_PC] = cpu->registers[REG2_INSTRU(instruction)];
}

static void jsr_jsrr(Cpu *cpu, uint16_t instruction) {
   uint16_t new_pc;
   cpu->registers[REG_R7] = cpu->registers[REG_PC];
   if (IS_JSR(instruction)) {
      new_pc = cpu->registers[REG_PC] + sign_extend(PCOFFSET11(instruction), 11);
   } else {
      new_pc = cpu->registers[REG_PC] + cpu->registers[REG2_INSTRU(instruction)];
   }
   cpu->registers[REG_PC] = new_pc;
}

static uint16_t compute_direct_address(Cpu *cpu, uint16_t instruction) {
   return cpu->registers[REG_PC] + sign_extend(PCOFFSET9(instruction), 9);
}

static uint16_t compute_indirect_address(Cpu *cpu, uint16_t instruction) {
   uint16_t pc_offset;
   pc_offset = sign_extend(PCOFFSET9(instruction), 9);
   return cpu->bus_access->read(cpu->bus_access, cpu->registers[REG_PC] + pc_offset);
}

static uint16_t compute_base_plus_offset(Cpu *cpu, uint16_t instruction) {
   uint16_t off_set = sign_extend(OFFSET6(instruction), 6);
   return cpu->registers[REG2_INSTRU(instruction)] + off_set;
}

static void ld(Cpu *cpu, uint16_t instruction) {
   int dr;
   uint16_t addr;
   dr = REG1_INSTRU(instruction);
   addr = compute_direct_address(cpu, instruction);
   cpu->registers[dr] = cpu->bus_access->read(cpu->bus_access, addr);
   set_condition_code(cpu, dr);
}

static void ldi(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_indirect_address(cpu, instruction);
   int dr = REG1_INSTRU(instruction);
   cpu->registers[dr] = cpu->bus_access->read(cpu->bus_access, addr);
   set_condition_code(cpu,dr);
}

static void ldr(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(cpu, instruction);
   int dr = REG1_INSTRU(instruction);
   cpu->registers[dr] = cpu->bus_access->read(cpu->bus_access, addr);
   set_condition_code(cpu, dr);
}

static void lea(Cpu *cpu, uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   cpu->registers[dr] = compute_direct_address(cpu, instruction);
   set_condition_code(cpu, dr);
}

static void not(Cpu *cpu, uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   int sr = REG2_INSTRU(instruction);
   cpu->registers[dr] = ~(cpu->registers[sr]);
   set_condition_code(cpu, dr);
}

static void rti(Cpu *cpu, uint16_t instruction) {
   if (SUPERVISOR_BIT(cpu->registers[REG_PSR])) {
      cpu->priv_mode_violation_exception_line.toggle = 1;
      return;
   }
   cpu->registers[REG_PC] = supervisor_stack_pop(cpu);
   cpu->registers[REG_PSR] = supervisor_stack_pop(cpu);
   if (!SUPERVISOR_BIT(cpu->registers[REG_PSR])) {
      cpu->registers[REG_R6] = cpu->registers[REG_USP];
   }
}

static void st(Cpu *cpu, uint16_t instruction) {
   int sr = REG1_INSTRU(instruction);
   uint16_t addr = compute_direct_address(cpu, instruction);
   cpu->bus_access->write(cpu->bus_access, addr, cpu->registers[sr]);
}

static void sti(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_indirect_address(cpu, instruction);
   int sr = REG1_INSTRU(instruction);
   cpu->bus_access->write(cpu->bus_access, addr, cpu->registers[sr]);
}

static void str(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(cpu, instruction);
   int sr = REG1_INSTRU(instruction);
   cpu->bus_access->write(cpu->bus_access, addr, cpu->registers[sr]);
}

static void trap(Cpu *cpu, uint16_t instruction) {
   uint16_t trapvector = TRAPVECT8(instruction);
   if (trapvector > 0x00FF) {
      /* ERROR */
   }
   cpu->registers[REG_R7] = cpu->registers[REG_PC];
   cpu->registers[REG_PC] = cpu->bus_access->read(cpu->bus_access, trapvector);
   /*psr &= 0x7FFF; mabye*/
}

static void load_instructions(void) {
   instru_func_vec[ADD]      = add_and;
   instru_func_vec[AND]      = add_and;
   instru_func_vec[BR]       = br;
   instru_func_vec[JMP_RET]  = jmp_ret;
   instru_func_vec[JSR_JSRR] = jsr_jsrr; 
   instru_func_vec[LD]       = ld;
   instru_func_vec[LDI]      = ldi;
   instru_func_vec[LDR]      = ldr;
   instru_func_vec[LEA]      = lea;
   instru_func_vec[NOT]      = not;
   instru_func_vec[RTI]      = rti;
   instru_func_vec[ST]       = st;
   instru_func_vec[STI]      = sti;
   instru_func_vec[STR]      = str;
   instru_func_vec[TRAP]     = trap;
}

static void cpu_execute_interrupt(Cpu *cpu, uint8_t vec_location, uint8_t priority) {
   uint16_t priority_extended;
   if (SUPERVISOR_BIT(cpu->registers[REG_PSR])) {
      cpu->registers[REG_USP] = cpu->registers[REG_R6];
      cpu->registers[REG_R6] = cpu->registers[REG_SSP];
   }
   supervisor_stack_push(cpu, cpu->registers[REG_PSR]);
   supervisor_stack_push(cpu, cpu->registers[REG_PC]);
   /* clear psr */
   cpu->registers[REG_PSR] = 0;
   /* set to supervisor mode */
   cpu->registers[REG_PSR] |= INIT_PSR_MASK_SUPERVISOR;
   /* set priority level */
   priority_extended = priority;
   cpu->registers[REG_PSR] |= (priority_extended << 8);
   /* set pc to interrupt vector */
   cpu->registers[REG_PC] = cpu->bus_access->read(cpu->bus_access, INTERRUPT_VECTOR_TABLE | vec_location);
}

static void cpu_execute_exception(Cpu *cpu, uint8_t vec_location) {
   uint16_t priority = 0x0007 & (cpu->registers[REG_PSR] >> 8);
   if (SUPERVISOR_BIT(cpu->registers[REG_PSR])) {
      cpu->registers[REG_USP] = cpu->registers[REG_R6];
      cpu->registers[REG_R6] = cpu->registers[REG_SSP];
   }
   supervisor_stack_push(cpu, cpu->registers[REG_PSR]);
   supervisor_stack_push(cpu, cpu->registers[REG_PC]);
   cpu->registers[REG_PSR] = 0;
   cpu->registers[REG_PSR] |= INIT_PSR_MASK_SUPERVISOR;
   cpu->registers[REG_PSR] |= (priority << 8);
   cpu->registers[REG_PC] = cpu->bus_access->read(cpu->bus_access, INTERRUPT_VECTOR_TABLE | vec_location);
}

static void cpu_check_exceptions(Cpu *cpu) {
   if (cpu->priv_mode_violation_exception_line.toggle) {
      cpu->priv_mode_violation_exception_line.toggle = 0;
      cpu_execute_exception(cpu, cpu->priv_mode_violation_exception_line.vec_location);
   } else if (cpu->illegal_opcode_exception_line.toggle) {
      cpu->illegal_opcode_exception_line.toggle = 0;
      cpu_execute_exception(cpu, cpu->illegal_opcode_exception_line.vec_location);
   }
}

int cpu_signal_interrupt(Cpu *cpu, uint8_t vec_location, uint8_t priority) {
   if (priority <= PSR_PRIORITY(cpu->registers[REG_PSR])) {
      return 0;
   }
   cpu_execute_interrupt(cpu, vec_location, priority);
   return 1;
}

void free_cpu(Cpu *cpu) {
   free(cpu);
}

uint16_t cpu_read_register(Cpu *cpu, enum lc3_reg reg) {
   return cpu->registers[reg];
}

void cpu_write_register(Cpu *cpu, enum lc3_reg reg, uint16_t value) {
   cpu->registers[reg] = value;
}

Cpu *new_Cpu(struct bus_accessor *bus_access) {
   Cpu *cpu;
   cpu = malloc(sizeof(Cpu));
   if (cpu == NULL) {
      return NULL;
   }
   cpu->bus_access = bus_access;
   setup_exceptions(cpu);
   if (!instructions_loaded) {
      load_instructions();
   }
   memset(cpu->registers, 0, sizeof(uint16_t) * num_registers);
   bus_access->write(bus_access, MCR_ADDR, 0x8000);
   return cpu;
}

int cpu_tick(Cpu *cpu) {
   instru_func func;
   int opcode;
   uint16_t instruction;
   if (!CLOCK_ENABLED(cpu->bus_access->read(cpu->bus_access, MCR_ADDR))) {
      return 0;
   }
   instruction = cpu->bus_access->read(cpu->bus_access, cpu->registers[REG_PC]);
   ++cpu->registers[REG_PC];
   opcode = OPCODE(instruction);
   if (opcode > 15 || opcode == 13) {
      cpu->illegal_opcode_exception_line.toggle = 1;
   } else {
      func = instru_func_vec[opcode];
      func(cpu, instruction);
   }
   if (!CLOCK_ENABLED(cpu->bus_access->read(cpu->bus_access, MCR_ADDR))) {
      return 0;
   }
   cpu_check_exceptions(cpu);
   return 1;
}
