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
#include "io.h"
#include "keyboard.h"
#include "display.h"
#include "machine_control.h"
#include "terminal.h"
#include "list.h"

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

typedef void (*instru_func)(Cpu *, uint16_t);
typedef int (*exception_line)(uint8_t *); 

struct cpu_exception {
   int toggle;
   uint8_t vec_location;
};

struct Cpu {
   Memory *memory;
   struct cpu_exception priv_mode_violation_exception_line;
   struct cpu_exception illegal_opcode_exception_line;
   List *interrupt_lines;
   List *io_devices;
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

/* TODO: fix return values of this function and don't exit
 * from this function
 */ 
static int init_program(Cpu *cpu, char *program_name) {
   FILE *program_file;
   int err;
   program_file = fopen(program_name, "r");
   if (program_file == NULL) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   cpu->pc = load_program(cpu->memory, program_file, &err);
   if (err) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   cpu->psr = INIT_PSR_MASK_USER;
   return 0;
}

static void load_os(Cpu *cpu) {
   DIR *dir;
   struct dirent *dp;
   FILE *curFile;
   struct stat file_stat;
   char *directory = "os/";
   char path[4 + NAME_MAX + 1];
   char *fullPath = NULL;
   int err;
   dir = opendir("os/");
   if (dir == NULL) {
      perror("No os");
      exit(EXIT_FAILURE);
   }
   while ((dp = readdir(dir)) != NULL) {
      strcpy(path, directory);
      strncat(path, dp->d_name, (sizeof(path) - 5) - 1);
      fullPath = realpath(path, NULL);
      if (stat(fullPath, &file_stat) < 0) {
         perror("stat");
         exit(EXIT_FAILURE);
      }
      if (S_ISDIR(file_stat.st_mode)) {
         continue;
      }
      curFile = fopen(fullPath, "r");
      if (curFile == NULL) {
         perror("open");
         exit(EXIT_FAILURE);
      }
      free(fullPath);
      load_program(cpu->memory, curFile, &err);
      if (err) {
         perror(NULL);
         exit(EXIT_FAILURE);
      }
   }
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
   //printf("Branhc\n");
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

static int attach_device(Cpu *cpu, struct io_device *io_device) {
   int i;
   for (i = 0; i < io_device->num_io_registers; ++i) {
      memory_register_io_registers(cpu->memory, io_device->io_registers, io_device->num_io_registers);
      if (io_device->io_interrupt != NULL) {
         if (list_add(cpu->interrupt_lines, io_device->io_interrupt) < 0) {
            return -1;
         }
      }
   } 
   return 0;
}

int cpu_attach_devices(Cpu *cpu, struct io_device **devices, int num_devices) {
   int i;
   for (i = 0; i < num_devices; ++i) {
      attach_device(cpu, devices[i]);
      if (list_add(cpu->io_devices, devices[i]) < 0) {
         return -1;
      }
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

static void check_interrupts_and_exceptions(Cpu *cpu) {
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
   num_interrupt_lines = list_size(cpu->interrupt_lines);
   for (i = 0; i < num_interrupt_lines; ++i) {
      interrupt_line = list_get(cpu->interrupt_lines, i);
      if (interrupt_line->interrupt_line_status(interrupt_line)) {
         if (PRIORITY_CMP(cpu->psr, interrupt_line->priority)) {
            continue;
         }
         execute_interrupt(cpu, interrupt_line->vec_location, interrupt_line->priority);
         return;
      }
   }
}

static int setup_mcr(Cpu *cpu) {
   struct io_device *mcr = mcr_get_device();
   if (mcr == NULL) {
      return -1;
   }
   memory_register_io_registers(cpu->memory, mcr->io_registers, mcr->num_io_registers);
   list_add(cpu->io_devices, mcr);
   return 0;
}

void free_cpu(Cpu *cpu) {

}

Cpu *new_Cpu(char *program_name) {
   Cpu *cpu = calloc(1, sizeof(struct Cpu));
   if (cpu == NULL) {
      return NULL;
   }
   cpu->memory = new_Memory();
   if (cpu->memory == NULL) {
      free(cpu);
      return NULL;
   }
   cpu->interrupt_lines = new_List(4, 0.5);
   if (cpu->interrupt_lines == NULL) {
      return NULL;
   }
   cpu->io_devices = new_List(4, 0.5);
   if (cpu->io_devices == NULL) {
      return NULL;
   }
   load_os(cpu);
   init_program(cpu, program_name);
   setup_exceptions(cpu);
   if (!instructions_loaded) {
      load_instructions();
   }
   if (setup_mcr(cpu) < 0) {
      free_cpu(cpu);
      return NULL;
   }
   return cpu;
}

void cpu_execute_until_end(Cpu *cpu) {
   instru_func func;
   while (CLOCK_ENABLED(read_memory(cpu->memory, 0xFFFE))) {
      int opcode;
      uint16_t instruction;
      instruction = read_memory(cpu->memory, cpu->pc++);
      opcode = OPCODE(instruction);
      if (opcode > 15 || opcode == 13) {
         cpu->illegal_opcode_exception_line.toggle = 1;
      }
      func = instru_func_vec[opcode];
      func(cpu, instruction);
      check_interrupts_and_exceptions(cpu);
   }
}
