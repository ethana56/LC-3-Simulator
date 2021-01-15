#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include "io.h"
#include "cpu.h"
#include "util.h"
#include "terminal.h"

#define NOT_READY    0x7FFF
#define KBSR         0xFE00
#define KBDR         0xFE02
#define VEC_LOCATION 0x80
#define PRIORITY     PL4

#define INTERRUPT_BIT(status) (((status) >> 14) & 0x4000)
#define READY_BIT(status) ((status) >> 15)
#define SET_READY_BIT 0x8000

static uint16_t read_kbsr(struct io_register *);
static uint16_t read_kbdr(struct io_register *);
static void write_kbsr(struct io_register *, uint16_t);
static int read_interrupt_line(struct io_interrupt_line *);

struct kb_data {
    uint16_t kbsr_internal;
    uint16_t kbdr_internal;
    int interrupt_line_toggle;
};

static struct kb_data  kb_data_internal = {
    .kbsr_internal = 0,
    .interrupt_line_toggle = 0
};

static struct io_register kbsr_external = {
    .data = &kb_data_internal,
    .address = KBSR,
    .read = read_kbsr,
    .write = write_kbsr
};

static struct io_register kbdr_external = {
    .data = &kb_data_internal,
    .address = KBDR,
    .read = read_kbdr,
    .write = NULL
};

static struct io_interrupt_line kb_interrupt_line = {
    .data = &kb_data_internal,
    .interrupt_line_status = read_interrupt_line,
    .vec_location = VEC_LOCATION,
    .priority = PRIORITY
};

static struct io_register *kb_registers[2] = {&kbsr_external, &kbdr_external};

static struct io_device kb_device = {
    .io_registers = kb_registers,
    .io_interrupt = &kb_interrupt_line,
    .num_io_registers = 2
};

static void write_kbsr(struct io_register *kbsr_io_register, uint16_t value) {
    struct kb_data *data = kbsr_io_register->data;
    data->kbsr_internal = value;
}

static uint16_t read_kbdr(struct io_register *kbdr_io_register) {
    struct kb_data *data = kbdr_io_register->data;
    unsigned char byte;
    data->kbsr_internal &= NOT_READY;
    if (read(STDIN_FILENO, &byte, 1) < 0) {
        return data->kbdr_internal;
    }
    data->kbdr_internal = byte;
    if (tcflush(STDIN_FILENO, TCIFLUSH) < 0) {
        fprintf(stderr, "tcflush error: %s\n", strerror(errno));
    }
    data->interrupt_line_toggle = 0;
    return data->kbdr_internal;
}

static fd_set kbsr_set;
static int fdset_set = 0;
static struct timeval tv;

static uint16_t read_kbsr_internal(struct kb_data *data) {
    int result;
    fd_set local_set;
    if (READY_BIT(data->kbsr_internal)) {
        return data->kbsr_internal;
    }
    local_set = kbsr_set;
    while ((result = select(STDIN_FILENO + 1, &kbsr_set, NULL ,NULL, &tv)) < 0) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }
    kbsr_set = local_set;
    if (result > 0) {
        data->kbsr_internal |= SET_READY_BIT;
        if (INTERRUPT_BIT(data->kbsr_internal)) {
            data->interrupt_line_toggle = 1;
        }
    }
    return data->kbsr_internal;
}

static uint16_t read_kbsr(struct io_register *kbsr_io_register) {
    return read_kbsr_internal(kbsr_io_register->data);
}

static int read_interrupt_line(struct io_interrupt_line *kb_interrupt_line) {
    int ret = 0;
    struct kb_data *data = kb_interrupt_line->data;
    //read_kbsr_internal(data);
    if (data->interrupt_line_toggle) {
        ret = 1;
    }
    data->interrupt_line_toggle = 0;
    return ret;
}

interrupt_line keyboard_get_interrupt_line(void) {
    return read_interrupt_line;
}

struct io_device *keyboard_get_device(void) {
    static int set_nonblock_bool = 0;
    if (init_terminal() < 0) {
        return NULL;
    }
    if (!fdset_set) {
        FD_ZERO(&kbsr_set);
        FD_SET(STDIN_FILENO, &kbsr_set);
        memset(&tv, 0, sizeof(tv));
        fdset_set = 1;
    }
    if (!set_nonblock_bool) {
        if (set_nonblock(STDIN_FILENO) < 0) {
            return NULL;
        }
        set_nonblock_bool = 1;
    }
    return &kb_device;
}






