#ifndef IO_H
#define IO_H

#include <stdint.h>

struct io_interrupt_line;
struct io_register;
typedef int (*interrupt_line)(struct io_interrupt_line *);
typedef uint16_t (*io_register_in) (struct io_register *);
typedef void (*io_register_out) (struct io_register *, uint16_t);

struct io_register {
   void *data;
   uint16_t address;
   io_register_in  read;
   io_register_out write;
};

struct io_interrupt_line {
   void *data;
   interrupt_line interrupt_line_status;
   uint8_t vec_location;
   uint8_t priority;
};

struct io_device {
   struct io_register **io_registers;
   struct io_interrupt_line *io_interrupt;
   int num_io_registers;
};

#endif
