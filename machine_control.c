#include <stdlib.h>
#include <stdint.h>
#include "io.h"

#define MCR 0xFFFE

static uint16_t mcr_internal = 0x8000;

static uint16_t read_mcr(void);
static void write_mcr(uint16_t);

static struct io_register mcr_external = {
    .address = MCR,
    .read = read_mcr,
    .write = write_mcr
};

static uint16_t read_mcr(void) {
    return mcr_internal;
}

static void write_mcr(uint16_t new_mcr) {
    mcr_internal = new_mcr;
}

void mcr_get_register(struct io_register **mcr) {
    *mcr = &mcr_external;
}
