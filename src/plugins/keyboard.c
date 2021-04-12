#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>

#include "device.h"

#define KEYBOARD_INTERRUPT_VECTOR   0x80
#define KEYBOARD_INTERRUPT_PRIORITY 4

#define SET_READY_BIT(reg) (0x8000 & (reg))
#define INTERRUPT_ENABLE_BIT_ON(reg) (0x4000 & (reg))

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0X7FFF

#define KBSR 0xFE00
#define KBDR 0xFE02

#define KBSR_INIT 0
#define KBDR_INIT 0

static const uint16_t keyboard_addresses[] = {KBSR, KBDR};
static const size_t keyboard_num_addresses = 2;
static const enum address_method keyboard_method = SEPERATE;

struct keyboard_data {
    struct host *host;
    uint16_t kbsr;
    uint16_t kbdr;
};

static uint16_t keyboard_read_register(struct device *keyboard_device, uint16_t address) {
    struct keyboard_data *keyboard_data;
    uint16_t value;
    keyboard_data = keyboard_device->data;
    switch (address) {
    case KBDR:
        keyboard_data->kbsr &= READY_BIT_SET_OFF;
        value = keyboard_data->kbdr;    
        break;
    case KBSR:
        value = keyboard_data->kbsr;
        break;    
    }
    return value;
}

static void keyboard_write_register(struct device *keyboard_device, uint16_t address, uint16_t value) {
    struct keyboard_data *keyboard_data;
    uint16_t kbsr_ready_bit;
    keyboard_data = keyboard_device->data;
    switch (address) {
    case KBSR:
        kbsr_ready_bit = keyboard_data->kbsr & READY_BIT_SET_ON;
        keyboard_data->kbsr = value;
        keyboard_data->kbsr |= kbsr_ready_bit;
        break;
    case KBDR:
        break;    
    }
}

static void keyboard_on_input(struct device *keyboard_device, uint16_t input) {
    struct keyboard_data *keyboard_data;
    keyboard_data = keyboard_device->data;
    keyboard_data->kbdr = input;
    keyboard_data->kbsr |= READY_BIT_SET_ON;
    if (INTERRUPT_ENABLE_BIT_ON(keyboard_data->kbsr)) {
        keyboard_data->host->alert_interrupt(keyboard_data->host, KEYBOARD_INTERRUPT_VECTOR, KEYBOARD_INTERRUPT_PRIORITY);
    }
}

static void keyboard_free(struct device *keyboard_device) {
    free(keyboard_device->data);
    free(keyboard_device);
}

static const uint16_t *keyboard_get_addresses(struct device *keyboard_device, size_t *num_addresses) {
    *num_addresses = keyboard_num_addresses;
    return keyboard_addresses;
}

static enum address_method keyboard_get_address_method(struct device *keyboard_device) {
    return keyboard_method;
}

static void keyboard_start(struct device *keyboard_device, struct host *host) {
    struct keyboard_data *keyboard_data;
    keyboard_data = keyboard_device->data;
    keyboard_data->host = host;
}

static void init_keyboard_device(struct device *keyboard_device, struct keyboard_data *keyboard_data) {
    keyboard_data->host = NULL;
    keyboard_data->kbdr = KBDR_INIT;
    keyboard_data->kbsr = KBSR_INIT;
    keyboard_device->data = keyboard_data;
    keyboard_device->read_register = keyboard_read_register;
    keyboard_device->write_register = keyboard_write_register;
    keyboard_device->on_input = keyboard_on_input;
    keyboard_device->on_tick = NULL;
    keyboard_device->start = keyboard_start;
    keyboard_device->free = keyboard_free;
    keyboard_device->get_addresses = keyboard_get_addresses;
    keyboard_device->get_address_method = keyboard_get_address_method;
}

struct device *init_device_plugin(void) {
    struct device *keyboard_device;
    struct keyboard_data *keyboard_data;
    keyboard_device = malloc(sizeof(struct device));
    if (keyboard_device == NULL) {
        return NULL;
    }
    keyboard_data = malloc(sizeof(struct keyboard_data));
    if (keyboard_data == NULL) {
        free(keyboard_device);
        return NULL;
    }
    init_keyboard_device(keyboard_device, keyboard_data);
    return keyboard_device;
}