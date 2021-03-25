#ifndef INTERRUPT_CONTROLLER_H
#define INTERRUPT_CONTROLLER_H

#include<stdint.h>

struct interrupt_controller;
typedef struct interrupt_controller InterruptController;

InterruptController *interrupt_controller_new(void);
void interrupt_controller_free(InterruptController *);
void interrupt_controller_alert(InterruptController *, uint8_t, uint8_t);
int interrupt_controller_check(InterruptController *, uint8_t, uint8_t *, uint8_t *, int (*)(uint8_t, uint8_t));
int interrupt_controller_peek(InterruptController *, uint8_t *, uint8_t *);
void print_members(InterruptController *);
int check_heap(InterruptController *);
#endif