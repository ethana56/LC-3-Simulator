#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "device_plugin.h"

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0x7FFF

#define DSR 0xFE04
#define DDR 0xFE06

static uint16_t read_register(uint16_t);
static void write_register(uint16_t, uint16_t);
static void free_display(struct device_plugin *);

static uint16_t dsr = 0x8000;
static uint16_t ddr = 0;

static uint16_t addresses[] = {DSR, DDR};
static const size_t num_addresses = 2;
static const enum address_method method = SEPERATE;


static struct host *host_funcs;
static struct device_plugin display_plugin = {
    .read_register = read_register,
    .write_register = write_register,
    .on_input = NULL,
    .on_tick = NULL,
    .free = free_display,
    .addresses = addresses,
    .num_addresses = num_addresses,
    .method = method
};

static uint16_t read_register(uint16_t address) {
    switch (address) {
    case DSR:
        return dsr;
    case DDR:
        return ddr;    
    }
}

static void write_register(uint16_t address, uint16_t value) {
    uint16_t dsr_ready_bit;
    switch (address) {
    case DSR:
        dsr_ready_bit = dsr & READY_BIT_SET_ON;
        dsr = value;
        dsr |= dsr_ready_bit;
        break;
    case DDR:
        host_funcs->write_output(host_funcs, value);
        break;
    }
}

static void free_display(struct device_plugin *display) {
    return;
}

struct device_plugin *init_device_plugin(struct host *host) {
    host_funcs = host;
    return &display_plugin;
}