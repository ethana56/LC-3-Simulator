#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "device_plugin.h"

#define KEYBOARD_INTERRUPT_VECTOR   0x80
#define KEYBOARD_INTERRUPT_PRIORITY 4

#define SET_READY_BIT(reg) (0x8000 & (reg))
#define INTERRUPT_ENABLE_BIT_ON(reg) (0x4000 & (reg))

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0X7FFF

#define KBSR 0xFE00
#define KBDR 0xFE02

static uint16_t read_register(uint16_t);
static void write_register(uint16_t, uint16_t);
static void on_input(char);
static void free_keyboard(struct device_plugin *);

static uint16_t kbsr = 0;
static uint16_t kbdr = 0;

static uint16_t addresses[] = {KBSR, KBDR};
static const size_t num_addresses = 2;
static const enum address_method method = SEPERATE;

static struct host *host_funcs;
static struct device_plugin keyboard_plugin = {
    .read_register = read_register,
    .write_register = write_register,
    .on_input = on_input,
    .on_tick = NULL,
    .free = free_keyboard,
    .addresses = addresses,
    .num_addresses = num_addresses,
    .method = method
};

static void on_input(char input) {
    kbdr = input;
    kbsr |= READY_BIT_SET_ON;
    if (INTERRUPT_ENABLE_BIT_ON(kbsr)) {
        host_funcs->alert_interrupt(host_funcs, KEYBOARD_INTERRUPT_VECTOR, KEYBOARD_INTERRUPT_PRIORITY);
    }
}

static uint16_t read_register(uint16_t address) {
    switch (address) {
    case KBDR:
        kbsr &= READY_BIT_SET_OFF;
        return kbdr;
    case KBSR:
        return kbsr;        
    }
}

static void write_register(uint16_t address, uint16_t value) {
    uint16_t kbsr_ready_bit;
    switch (address) {
        case KBSR:
            kbsr_ready_bit = kbsr & READY_BIT_SET_ON;
            kbsr = value;
            kbsr |= kbsr_ready_bit;
            break;
        case KBDR:
            break;    
    }
}

static void free_keyboard(struct device_plugin *keyboard) {
    return;
}

struct device_plugin *init_device_plugin(struct host *host) {
    host_funcs = host;
    return &keyboard_plugin;
}
