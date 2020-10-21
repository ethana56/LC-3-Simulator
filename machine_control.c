#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "io.h"

#define MCR 0xFFFE
#define INIT_MCR 0x8000

static uint16_t read_mcr(struct io_register *mcr) {
    uint16_t *data = mcr->data;
    return *data;
}

static void write_mcr(struct io_register *mcr, uint16_t new_mcr) {
    uint16_t *data = mcr->data;
    *data = new_mcr;
}

struct io_device *mcr_get_device(void) {
    struct io_device *mcr = malloc(sizeof(struct io_device));
    uint16_t *data;
    if (mcr == NULL) {
        return NULL;
    }
    mcr->io_registers = malloc(sizeof(struct io_register *));
    if (mcr->io_registers == NULL) {
        free(mcr);
        return NULL;
    }
    mcr->io_registers[0] = malloc(sizeof(struct io_register));
    if (mcr->io_registers[0] == NULL) {
        free(mcr->io_registers);
        free(mcr);
        return NULL;
    }
    mcr->io_registers[0]->address = MCR;
    data = malloc(sizeof(uint16_t));
    if (data == NULL) {
        free(mcr->io_registers);
        free(mcr);
        return NULL;
    }
    *data = INIT_MCR;
    mcr->io_registers[0]->data = data;
    mcr->io_registers[0]->read = read_mcr;
    mcr->io_registers[0]->write = write_mcr;
    mcr->io_interrupt = NULL;
    mcr->num_io_registers = 1;
    return mcr;
}

void mcr_free(struct io_device *mcr) {
    free(mcr->io_registers[0]->data);
    free(mcr->io_registers[0]);
    free(mcr->io_registers);
    free(mcr);
}
