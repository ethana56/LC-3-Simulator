#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "util.h"
#include "io.h"

#define IO_START_ADDRESS     0xFE00

#define NUM_ADDRESSES 65536
#define MEM_MAPPED_ADDRESSES 512
#define IS_IO(address) (((address) >= 0xFE00) && ((address) <= 0xFFFF))
#define IS_IN_RANGE(addr) ((addr) >= 65024)
#define IO_ADDRESS(addr) ((addr) - IO_START_ADDRESS)

#define LOW_PROG_ADDR  0x3000
#define HIGH_PROG_ADDR 0xFDFF


static uint16_t mem[NUM_ADDRESSES];
static struct io_register *mem_mapped_io[MEM_MAPPED_ADDRESSES];

static struct io_register *get_io_register(uint16_t address) {
   return mem_mapped_io[IO_ADDRESS(address)];
}

uint16_t read_memory(uint16_t address) {
   uint16_t result;
   if (IS_IO(address)) {
      struct io_register *io_register = get_io_register(address);
      if (io_register == NULL || io_register->read == NULL) { 
         result = 0x0000;
      } else {
         result = io_register->read();
      }
   } else {
      result = mem[address];
   }
   return result;
}

uint16_t load_program(FILE *program_file, int *err) {
   uint16_t addr;
   size_t result;
   *err = 0;
   if (read_convert_16bits(&addr, 1, program_file) == 0) {
      if (ferror(program_file)) {
         *err = 1;
         return 0;
      }
   }
   result = read_convert_16bits(&mem[addr], HIGH_PROG_ADDR - addr, program_file);
   if (result != HIGH_PROG_ADDR - addr && ferror(program_file)) {
      *err = 1;
      return 0;
   }
   return addr;
}

/*uint16_t load_program(FILE *program_file, int user) {
   uint16_t addr;
   size_t result;
   if (read_convert_16bits(&addr, 1, program_file) == 0) {
      if (ferror(program_file)) {
         perror(NULL);
      } else {
         fprintf(stderr, "No program stored\n");
      }
      exit(EXIT_FAILURE);
   }
   if (user && (addr < LOW_PROG_ADDR || addr > HIGH_PROG_ADDR)) {
      fprintf(stderr, "invalid starting address\n");
      exit(EXIT_FAILURE);
   }
   result = read_convert_16bits(&mem[addr], HIGH_PROG_ADDR - addr, program_file);
   if (result != HIGH_PROG_ADDR - addr && ferror(program_file)) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   return addr;
}*/

void write_memory(uint16_t address, uint16_t value) {
   if (IS_IO(address)) {
      struct io_register *io_register = get_io_register(address);
      if (io_register == NULL || io_register ->write == NULL) {
         return;
      }
      io_register->write(value);
   } else {
      mem[address] = value;
   }
}

void memory_register_io_registers(struct io_register **io_registers, int amt) {
   while (amt--) {
      if (!IS_IN_RANGE((*io_registers)->address)) {
         fprintf(stderr, "device address out of range.\n");
      } else {
         mem_mapped_io[IO_ADDRESS((*io_registers)->address)] = *io_registers;
      }
      ++io_registers;
   }
}
