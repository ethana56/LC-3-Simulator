#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "io.h"

int keyboard_get_registers(struct io_register **, struct io_register **);
interrupt_line keyboard_get_interrupt_line(void);

#endif
