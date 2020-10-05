#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "io.h"
#include "util.h"

#define DSR 0xFE04
#define DDR 0xFE06

static uint16_t dsr_internal = 0x8000;

static uint16_t read_dsr(void);
static void write_ddr(uint16_t);

static struct io_register dsr_external = {
    .address = DSR,
    .read = read_dsr,
    .write = NULL
};

static struct io_register ddr_external = {
    .address = DDR,
    .read = NULL,
    .write = write_ddr
};

static uint16_t read_dsr(void) {
    /* TODO: implement a timer to simulate output delay */
    return dsr_internal;
}

static void write_ddr(uint16_t value) {
    ssize_t result;
    unsigned char val;
    if (!(dsr_internal >> 15)) {
        return;
    }
    memcpy(&val, &value, 1);
    while ((result = write(STDOUT_FILENO, &val, 1)) == 0);
    if (result < 0) {
        fprintf(stderr, "terminal write error\n");
    }
    /* TODO: set dsr */
}

int display_get_registers(struct io_register **ddr, struct io_register **dsr) {
    if (set_nonblock(STDOUT_FILENO) < 0) {
        return -1;
    }
    *ddr = &ddr_external;
    *dsr = &dsr_external;
    return 0;
}
