#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "io.h"
#include "util.h"

#define DSR 0xFE04
#define DDR 0xFE06

static uint16_t dsr_internal = 0x8000;

static uint16_t read_dsr(struct io_register *);
static void write_ddr(struct io_register *, uint16_t);

static struct io_register dsr_external = {
    .data = &dsr_internal,
    .address = DSR,
    .read = read_dsr,
    .write = NULL
};

static struct io_register ddr_external = {
    .data = &dsr_internal,
    .address = DDR,
    .read = NULL,
    .write = write_ddr
};

static struct io_register *display_io_registers[2] = {&dsr_external, &ddr_external};

struct io_device display_device = {
    .io_registers = display_io_registers,
    .io_interrupt = NULL,
    .num_io_registers = 2
};

static uint16_t read_dsr(struct io_register *dsr) {
    /* TODO: implement a timer to simulate output delay */
    uint16_t *data = dsr->data;
    return *data;
}

static void write_ddr(struct io_register *ddr, uint16_t value) {
    uint16_t *data = ddr->data;
    ssize_t result;
    unsigned char val;
    if (!(*data >> 15)) {
        return;
    }
    memcpy(&val, &value, 1);
    while ((result = write(STDOUT_FILENO, &val, 1)) == 0);
    if (result < 0) {
        fprintf(stderr, "terminal write error\n");
    }
    /* TODO: set dsr */
}

struct io_device *display_get_device(void) {
    static int set_nonblock_bool = 0;
    if (!set_nonblock_bool) {
        if (set_nonblock(STDOUT_FILENO) < 0) {
            return NULL;
        }
        set_nonblock_bool = 1;
    }
    return &display_device;
}
