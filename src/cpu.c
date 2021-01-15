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
#include "memory.h"

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

#define PRIORITY_CMP(psr, priority) ((0x0007 & ((psr) >> 7)) > priority)

#define PRIV_MODE_VIOLATION_EXCEPTION_VECTOR 0x00
#define ILLEGAL_OPCODE_EXCEPTION_VECTOR      0x01

#define MCR_ADDR 0xFFFE

typedef void (*instru_func)(Cpu *, uint16_t);
typedef int (*exception_line)(uint8_t *); 

struct cpu_exception {
   int toggle;
   uint8_t vec_location;
};

struct Cpu {
   Memory *memory;
   struct io_register *io_registers;
   struct mcr {
       struct io_register mcr_register;
       uint16_t mcr_data;
   } mcr;
   struct cpu_exception priv_mode_violation_exception_line;
   struct cpu_exception illegal_opcode_exception_line;
   struct interrupt_controller *interrupt_controller;
   uint16_t registers[num_registers];
   uint16_t pc;
   uint16_t psr;
   struct {
      uint16_t usp;
      uint16_t ssp;
   } saved;
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
   cpu->registers[r6] -= 1;
   write_memory(cpu->memory, cpu->registers[r6], data);
}

static uint16_t supervisor_stack_pop(Cpu *cpu) {
   uint16_t data = read_memory(cpu->memory, cpu->registers[r6]);
   cpu->registers[r6] += 1;
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
   //printf("VAL: %d\n\r", val);
   cpu->psr &= NZP_PSR_CLEAR_MASK;
   if (val > 0) {
      cpu->psr |= SET_PSR_P_MASK;
   } else if (val < 0) {
      cpu->psr |= SET_PSR_N_MASK;
   } else {
      cpu->psr |= SET_PSR_Z_MASK;
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
   unsigned nzp_psr    = NZP_PSR(cpu->psr);
   if (nzp_instru & nzp_psr) {
      pc_offset = sign_extend(PCOFFSET9(instruction), 9);
      cpu->pc += pc_offset;
   }
}

static void jmp_ret(Cpu *cpu, uint16_t instruction) {
   cpu->pc = cpu->registers[REG2_INSTRU(instruction)];
}

static void jsr_jsrr(Cpu *cpu, uint16_t instruction) {
   uint16_t new_pc;
   cpu->registers[r7] = cpu->pc;
   if (IS_JSR(instruction)) {
      new_pc = cpu->pc + sign_extend(PCOFFSET11(instruction), 11);
   } else {
      new_pc = cpu->pc + cpu->registers[REG2_INSTRU(instruction)];
   }
   cpu->pc = new_pc;
}

static uint16_t compute_direct_address(Cpu *cpu, uint16_t instruction) {
   return cpu->pc + sign_extend(PCOFFSET9(instruction), 9);
}

static uint16_t compute_indirect_address(Cpu *cpu, uint16_t instruction) {
   uint16_t pc_offset = sign_extend(PCOFFSET9(instruction), 9);
   return read_memory(cpu->memory, cpu->pc + pc_offset);
}

static uint16_t compute_base_plus_offset(Cpu *cpu, uint16_t instruction) {
   uint16_t off_set = sign_extend(OFFSET6(instruction), 6);
   return cpu->registers[REG2_INSTRU(instruction)] + off_set;
}

static void ld(Cpu *cpu, uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   uint16_t addr = compute_direct_address(cpu, instruction);
   cpu->registers[dr] = read_memory(cpu->memory, addr);
   set_condition_code(cpu, dr);
}

static void ldi(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_indirect_address(cpu, instruction);
   int dr = REG1_INSTRU(instruction);
   cpu->registers[dr] = read_memory(cpu->memory, addr);
   set_condition_code(cpu,dr);
}

static void ldr(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(cpu, instruction);
   int dr = REG1_INSTRU(instruction);
   cpu->registers[dr] = read_memory(cpu->memory, addr);
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
   if (SUPERVISOR_BIT(cpu->psr)) {
      cpu->priv_mode_violation_exception_line.toggle = 1;
      return;
   }
   cpu->pc = supervisor_stack_pop(cpu);
   cpu->psr = supervisor_stack_pop(cpu);
   if (!SUPERVISOR_BIT(cpu->psr)) {
      cpu->registers[r6] = cpu->saved.usp;
   }
}

static void st(Cpu *cpu, uint16_t instruction) {
   int sr = REG1_INSTRU(instruction);
   uint16_t addr = compute_direct_address(cpu, instruction);
   write_memory(cpu->memory, addr, cpu->registers[sr]);
}

static void sti(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_indirect_address(cpu, instruction);
   int sr = REG1_INSTRU(instruction);
   write_memory(cpu->memory, addr, cpu->registers[sr]);
}

static void str(Cpu *cpu, uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(cpu, instruction);
   int sr = REG1_INSTRU(instruction);
   write_memory(cpu->memory, addr, cpu->registers[sr]);
}

static void trap(Cpu *cpu, uint16_t instruction) {
   uint16_t trapvector = TRAPVECT8(instruction);
   if (trapvector > 0x00FF) {
      /* ERROR */
   }
   cpu->registers[r7] = cpu->pc;
   cpu->pc = read_memory(cpu->memory, trapvector);
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

/*static void print_registers(void) {
   printf("r0: 0x%04X\nr1: 0x%04X\nr2: 0x%04X\nr3: 0x%04X\nr4: 0x%04X\nr5: 0x%04X\nr6: 0x%04X\nr7: 0x%04X\n\n\n", 
       registers[0], registers[1], registers[2], registers[3], 
       registers[4], registers[5], registers[6], registers[7]);
}*/

static void io_register_write(struct io_register *io_register, uint16_t value) {
    struct device *device;
    device = io_register->data;
    device->write_register(device, io_register->address, value);
}

static uint16_t io_register_read(struct io_register *io_register) {
    struct device *device;
    device = io_register->data;
    return device->read_register(device, io_register->address);
}

static size_t count_device_registers(struct device **devices, size_t num_devices) {
    size_t num_registers;
    size_t i;
    for (i = 0, num_registers = 0; i < num_devices; ++i) {
        num_registers += devices[i]->num_readable;
	num_registers += devices[i]->num_writeable;
	num_registers += devices[i]->num_readable_writeable;
    }
    return num_registers;
} 

static size_t device_build_io_registers(struct io_register *io_registers, struct device *device) {
    size_t i, io_reg_index = 0;
    for (i = 0; i < device->num_readable; ++i) {
        io_registers[io_reg_index].data = device;
	io_registers[io_reg_index].address = device->readable[i];
	io_registers[io_reg_index].read = io_register_read;
	io_registers[io_reg_index].write = NULL;
	++io_reg_index;
    }
    for (i = 0; i < device->num_writeable; ++i) {
        io_registers[io_reg_index].data = device;
	io_registers[io_reg_index].address = device->writeable[i];
	io_registers[io_reg_index].read = NULL;
	io_registers[io_reg_index].write = io_register_write;
	++io_reg_index;
    }
    for (i = 0; i < device->num_readable_writeable; ++i) {
        io_registers[io_reg_index].data = device;
	io_registers[io_reg_index].address = device->readable_writeable[i];
	io_registers[io_reg_index].read = io_register_read;
	io_registers[io_reg_index].write = io_register_write;
	++io_reg_index;
    }
    return io_reg_index;
}

static void build_io_registers(struct io_register *io_registers, struct device **devices, size_t num_devices) {
    size_t io_reg_index, i;
    for (i = 0, io_reg_index = 0; i < num_devices; ++i) {
        size_t cur_device_num_reg;
	cur_device_num_reg = device_build_io_registers(io_registers + io_reg_index, devices[i]);
	io_reg_index += cur_device_num_reg;
    }
}

int cpu_attach_devices(Cpu *cpu, struct device **devices, size_t num_devices) {
    struct io_register *io_registers;
    size_t num_io_registers, i;
    num_io_registers = count_device_registers(devices, num_devices);
    cpu->io_registers = malloc(sizeof(struct io_register) * num_io_registers);
    if (cpu->io_registers == NULL) {
        return -1;
    }
    build_io_registers(cpu->io_registers, devices, num_devices);
    for (i = 0; i < num_io_registers; ++i) {
        memory_register_io_register(cpu->memory, cpu->io_registers + i);
    }
    return 0;
}

static void execute_interrupt(Cpu *cpu, uint8_t vec_location, uint8_t priority) {
   uint16_t priority_extended;
   if (SUPERVISOR_BIT(cpu->psr)) {
      cpu->saved.usp = cpu->registers[r6];
      cpu->registers[r6] = cpu->saved.ssp;
   }
   supervisor_stack_push(cpu, cpu->psr);
   supervisor_stack_push(cpu, cpu->pc);
   /* clear psr */
   cpu->psr = 0;
   /* set to supervisor mode */
   cpu->psr |= INIT_PSR_MASK_SUPERVISOR;
   /* set priority level */
   priority_extended = priority;
   cpu->psr |= (priority_extended << 7);
   /* set pc to interrupt vector */
  cpu->pc = read_memory(cpu->memory, INTERRUPT_VECTOR_TABLE | vec_location);
}

static void execute_exception(Cpu *cpu, uint8_t vec_location) {
   uint16_t priority = 0x0007 & (cpu->psr >> 7);
   if (SUPERVISOR_BIT(cpu->psr)) {
      cpu->saved.usp = cpu->registers[r6];
      cpu->registers[r6] = cpu->saved.ssp;
   }
   supervisor_stack_push(cpu, cpu->psr);
   supervisor_stack_push(cpu, cpu->pc);
   cpu->psr = 0;
   cpu->psr |= INIT_PSR_MASK_SUPERVISOR;
   cpu->psr |= (priority << 7);
   cpu->pc = read_memory(cpu->memory, INTERRUPT_VECTOR_TABLE | vec_location);
}

/*static void check_interrupts_and_exceptions(Cpu *cpu) {
   int i, num_interrupt_lines;
   struct io_interrupt_line *interrupt_line;
   if (cpu->priv_mode_violation_exception_line.toggle) {
      cpu->priv_mode_violation_exception_line.toggle = 0;
      execute_exception(cpu, cpu->priv_mode_violation_exception_line.vec_location);
      return;
   }
   if (cpu->illegal_opcode_exception_line.toggle) {
      cpu->illegal_opcode_exception_line.toggle = 0;
      execute_exception(cpu, cpu->illegal_opcode_exception_line.vec_location);
      return;
   }

   uint8_t priority;
   uint8_t vec_location;
   if (cpu->interrupt_controller->interrupt_line(cpu->interrupt_controller, &vec_location, &priority)) {
      execute_interrupt(cpu, vec_location, priority);
      return ;
   }
}*/

static uint16_t mcr_read(struct io_register *mcr_register) {
    Cpu *cpu;
    cpu = mcr_register->data;
    return cpu->mcr.mcr_data;
}

static void mcr_write(struct io_register *mcr_register, uint16_t value) {
    Cpu *cpu;
    cpu = mcr_register->data;
    cpu->mcr.mcr_data = value;
}

static void setup_mcr(Cpu *cpu) {
    cpu->mcr.mcr_data = 0;
    cpu->mcr.mcr_data |= 0x8000;
    cpu->mcr.mcr_register.data = cpu;
    cpu->mcr.mcr_register.address = MCR_ADDR;
    cpu->mcr.mcr_register.read = mcr_read;
    cpu->mcr.mcr_register.write = mcr_write;

    memory_register_io_register(cpu->memory, &cpu->mcr.mcr_register);
}

void free_cpu(Cpu *cpu) {
   free_memory(cpu->memory);
   free(cpu->io_registers);
   free(cpu);
}

uint16_t cpu_read_memory(Cpu *cpu, uint16_t address) {
    return read_memory(cpu->memory, address);
}

void cpu_write_memory(Cpu *cpu, uint16_t address, uint16_t value) {
    write_memory(cpu->memory, address, value);
}

void cpu_load_program(Cpu *cpu, const uint16_t *program, size_t amt) {
    size_t i;
    uint16_t cur_addr, max_addr;
    if (amt == 1) {
        return;
    }
    max_addr = MAX_PROGRAM_LEN - 1;
    cur_addr = program[0];
    for (i = 1; i < amt && cur_addr <= max_addr; ++i, ++cur_addr) {
        write_memory(cpu->memory, cur_addr, program[i]);
    }
}

Cpu *new_Cpu(struct interrupt_controller *interrupt_controller) {
   Cpu *cpu = calloc(1, sizeof(struct Cpu));
   if (cpu == NULL) {
      return NULL;
   }
   cpu->memory = new_Memory();
   if (cpu->memory == NULL) {
      free(cpu);
      return NULL;
   }
   setup_exceptions(cpu);
   if (!instructions_loaded) {
      load_instructions();
   }
   setup_mcr(cpu);
   cpu->interrupt_controller = interrupt_controller;
   return cpu;
}

void cpu_execute_until_end(Cpu *cpu) {
   instru_func func;
   while (CLOCK_ENABLED(cpu->mcr.mcr_data)) {
      int opcode;
      uint16_t instruction;
      instruction = read_memory(cpu->memory, cpu->pc++);
      opcode = OPCODE(instruction);
      if (opcode > 15 || opcode == 13) {
         cpu->illegal_opcode_exception_line.toggle = 1;
      }
      func = instru_func_vec[opcode];
      func(cpu, instruction);
      //check_interrupts_and_exceptions(cpu);
   }
}
