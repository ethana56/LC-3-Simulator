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
#include "memory.h"
#include "io.h"
#include "keyboard.h"
#include "display.h"
#include "machine_control.h"
#include "terminal.h"

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
#define PCOFFSET11(instruction)  ((instruction) & 0x08FF)
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

#define SUPERVISOR_BIT(psr) ((psr) >> 15)

#define INTERRUPT_VECTOR_TABLE 0x0100
#define SUPERVISOR_STACK_HIGH  0x2FFF

#define IS_IMM5(instruction) (((instruction) >> 5) & 0x0001)
#define IS_JSR(instruction)  (((instruction) >> 11) & 0x0001)

#define PRIORITY_CMP(psr, priority) ((0x0007 & ((psr) >> 7)) > priority)

typedef void (*instru_func)(uint16_t);
typedef int (*exception_line)(uint8_t *); 

enum reg {r0, r1, r2, r3, r4, r5, r6, r7, num_registers};
enum pl {PL7, PL6, PL5, PL4, PL3, PL2, PL1, PL0};

static uint16_t registers[num_registers];
static uint16_t pc;
static uint16_t psr;
static struct {
   uint16_t usp;
   uint16_t ssp;
} saved;

static instru_func instru_func_vec[16];

static int priv_mode_violation_toggle = 0;
static int illegal_opcode_toggle      = 0;

static void supervisor_stack_push(uint16_t data) {
   registers[r6] -= 1;
   write_memory(registers[r6], data);
}

static uint16_t supervisor_stack_pop(void) {
   uint16_t data = read_memory(registers[r6]);
   registers[r6] += 1;
   return data;
}

static void init_program(int argc, char **argv) {
   FILE *program_file;
   int err;
   if (argc < 2) {
      fprintf(stderr, "Need name of program file\n");
      exit(EXIT_FAILURE);
   }
   program_file = fopen(argv[1], "r");
   if (program_file == NULL) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   pc = load_program(program_file, &err);
   if (err) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   psr = INIT_PSR_MASK_USER;
}

static void load_os(void) {
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
      free(fullPath);
      if (S_ISDIR(file_stat.st_mode)) {
         continue;
      }
      curFile = fopen(fullPath, "r");
      if (curFile == NULL) {
         perror("open");
         exit(EXIT_FAILURE);
      }
      load_program(curFile, &err);
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

static void set_condition_code(int reg) {
   int16_t val = registers[reg];
   psr &= NZP_PSR_CLEAR_MASK;
   if (val > 0) {
      psr |= SET_PSR_P_MASK;
   } else if (val < 0) {
      psr |= SET_PSR_N_MASK;
   } else {
      psr |= SET_PSR_Z_MASK;
   }
}

static void add_and(uint16_t instruction) {
   int dest = REG1_INSTRU(instruction);
   uint16_t sr1_val = registers[REG2_INSTRU(instruction)];
   uint16_t sr2_val;
   if (IS_IMM5(instruction)) {
      sr2_val = sign_extend(IMM5(instruction), 5);
   } else {
      sr2_val = registers[REG3_INSTRU(instruction)];
   }
   if (OPCODE(instruction) == ADD) { 
      registers[dest] = sr1_val + sr2_val;
   } else if (OPCODE(instruction) == AND) {
      registers[dest] = sr1_val & sr2_val;
   }
   set_condition_code(dest);
}

static void br(uint16_t instruction) {
   uint16_t pc_offset;
   unsigned nzp_instru = NZP_INSTRU(instruction);
   unsigned nzp_psr    = NZP_PSR(psr);
   if (nzp_instru & nzp_psr) {
      pc_offset = sign_extend(PCOFFSET9(instruction), 9);
      pc += pc_offset;
   }
}

static void jmp_ret(uint16_t instruction) {
   pc = registers[REG2_INSTRU(instruction)];
}

static void jsr_jsrr(uint16_t instruction) {
   uint16_t new_pc;
   registers[r7] = pc;
   if (IS_JSR(instruction)) {
      new_pc = pc + sign_extend(PCOFFSET11(instruction), 11);
   } else {
      new_pc = registers[REG2_INSTRU(instruction)];
   }
   pc = new_pc;
}

static uint16_t compute_direct_address(uint16_t instruction) {
   return pc + sign_extend(PCOFFSET9(instruction), 9);
}

static uint16_t compute_indirect_address(uint16_t instruction) {
   uint16_t pc_offset = sign_extend(PCOFFSET9(instruction) ,9);
   return read_memory(pc + pc_offset);
}

static uint16_t compute_base_plus_offset(uint16_t instruction) {
   uint16_t off_set = sign_extend(OFFSET6(instruction), 6);
   return registers[REG2_INSTRU(instruction)] + off_set;
}

static void ld(uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   uint16_t addr = compute_direct_address(instruction);
   registers[dr] = read_memory(addr);
   set_condition_code(dr);
}

static void ldi(uint16_t instruction) {
   uint16_t addr = compute_indirect_address(instruction);
   int dr = REG1_INSTRU(instruction);
   registers[dr] = read_memory(addr);
   set_condition_code(dr);
}

static void ldr(uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(instruction);
   int dr = REG1_INSTRU(instruction);
   registers[dr] = read_memory(addr);
   set_condition_code(dr);
}

static void lea(uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   registers[dr] = compute_direct_address(instruction);
   set_condition_code(dr);
}

static void not(uint16_t instruction) {
   int dr = REG1_INSTRU(instruction);
   int sr = REG2_INSTRU(instruction);
   registers[dr] = ~(registers[sr]);
   set_condition_code(dr);
}

static void rti(uint16_t instruction) {
   if (SUPERVISOR_BIT(psr)) {
      priv_mode_violation_toggle = 1;
      return;
   }
   pc = supervisor_stack_pop();
   psr = supervisor_stack_pop();
   if (!SUPERVISOR_BIT(psr)) {
      registers[r6] = saved.usp;
   }
}

static void st(uint16_t instruction) {
   int sr = REG1_INSTRU(instruction);
   uint16_t addr = compute_direct_address(instruction);
   write_memory(addr, registers[sr]);
}

static void sti(uint16_t instruction) {
   uint16_t addr = compute_indirect_address(instruction);
   int sr = REG1_INSTRU(instruction);
   write_memory(addr, registers[sr]);
}

static void str(uint16_t instruction) {
   uint16_t addr = compute_base_plus_offset(instruction);
   int sr = REG1_INSTRU(instruction);
   write_memory(addr, registers[sr]);
}

static void trap(uint16_t instruction) {
   uint16_t trapvector = TRAPVECT8(instruction);
   if (trapvector > 0x00FF) {
      /* ERROR */
   }
   registers[r7] = pc;
   pc = read_memory(trapvector);
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

static void setup_io(void) {
   struct io_register *io_registers[2];
   keyboard_get_registers(&io_registers[0], &io_registers[1]);
   memory_register_io_registers(io_registers, 2);
   display_get_registers(&io_registers[0], &io_registers[1]);
   memory_register_io_registers(io_registers, 2);
   mcr_get_register(io_registers);
   memory_register_io_registers(io_registers, 1);
}

static interrupt_line interrupt_lines[1];
static int num_interrupt_lines = 1;

static exception_line exception_lines[2];
static int num_exception_lines = 2;

static int priv_mode_violation_exception_line(uint8_t *vec_location) {
   int toggle = priv_mode_violation_toggle;
   priv_mode_violation_toggle = 0;
   *vec_location = 0x00;
   return toggle;
}

static int illegal_opcode_exception_line(uint8_t *vec_location) {
   int toggle = illegal_opcode_toggle;
   illegal_opcode_toggle = 0;
   *vec_location = 0x01;
   return toggle;
}

static void setup_interrupts_and_exceptions(void) {
   saved.ssp = SUPERVISOR_STACK_HIGH + 1;
   interrupt_lines[0] = keyboard_get_interrupt_line();

   exception_lines[0] = priv_mode_violation_exception_line;
   exception_lines[1] = illegal_opcode_exception_line;
}

static void execute_interrupt(uint8_t vec_location, uint8_t priority) {
   uint16_t priority_extended;
   if (SUPERVISOR_BIT(psr)) {
      saved.usp = registers[r6];
      registers[r6] = saved.ssp;
   }
   supervisor_stack_push(psr);
   supervisor_stack_push(pc);
   /* clear psr */
   psr = 0;
   /* set to supervisor mode */
   psr |= INIT_PSR_MASK_SUPERVISOR;
   /* set priority level */
   priority_extended = priority;
   psr |= (priority_extended << 7);
   /* set pc to interrupt vector */
   pc = read_memory(INTERRUPT_VECTOR_TABLE | vec_location);
}

static void execute_exception(uint8_t vec_location) {
   uint16_t priority = 0x0007 & (psr >> 7);
   if (SUPERVISOR_BIT(psr)) {
      saved.usp = registers[r6];
      registers[r6] = saved.ssp;
   }
   supervisor_stack_push(psr);
   supervisor_stack_push(pc);
   psr = 0;
   psr |= INIT_PSR_MASK_SUPERVISOR;
   psr |= (priority << 7);
   pc = read_memory(INTERRUPT_VECTOR_TABLE | vec_location);
}

static void check_interrupts_and_exceptions(void) {
   int i;
   uint8_t vec_location, priority;
   for (i = 0; i < num_exception_lines; ++i) {
      if (exception_lines[i](&vec_location)) {
         execute_exception(vec_location);
         return;
      }
   }
   for (i = 0; i < num_interrupt_lines; ++i) {
      if (interrupt_lines[i](&vec_location, &priority)) {
         if (PRIORITY_CMP(psr, priority)) {
            continue;
         }
         execute_interrupt(vec_location, priority);
         return;
      }
   }
}

void init_cpu(int argc, char **argv) {
   if (init_terminal() < 0) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   load_os();
   init_program(argc, argv);
   load_instructions();
   setup_io();
   setup_interrupts_and_exceptions();
}

int main(int argc, char *argv[]) {
   instru_func func;
   init_cpu(argc, argv);
   while (1) {
      int opcode;
      uint16_t instruction; 
      instruction = read_memory(pc++);
      opcode = OPCODE(instruction);
      if (opcode > 15 || opcode == 13) {
         illegal_opcode_toggle = 1;
      }
      func = instru_func_vec[opcode];
      func(instruction);
      check_interrupts_and_exceptions();
   }
}

















