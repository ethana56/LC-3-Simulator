#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include "io.h"
#include "util.h"

#define NOT_READY    0x7FFF
#define KBSR         0xFE00
#define KBDR         0xFE02
#define VEC_LOCATION 0x80

#define INTERRUPT_BIT(status) (((status) >> 14) & 0x4000)
#define READY_BIT(status) ((status) >> 15)
#define SET_READY_BIT 0x8000

static uint16_t read_kbsr(void);
static uint16_t read_kbdr(void);
static void write_kbsr(uint16_t);

static uint16_t kbsr_internal = 0;
static int interrupt_line_toggle = 0;

static struct io_register kbsr_external = {
    .address = KBSR,
    .read = read_kbsr,
    .write = write_kbsr
};

static struct io_register kbdr_external = {
    .address = KBDR,
    .read = read_kbdr,
    .write = NULL
};

static void write_kbsr(uint16_t data) {
    kbsr_internal = data;
}

static uint16_t read_kbdr(void) {
    static uint16_t kbdr_internal;
    unsigned char byte;
    kbsr_internal &= NOT_READY;
    if (read(STDIN_FILENO, &byte, 1) < 0) {
        return kbdr_internal;
    }
    kbdr_internal = byte;
    if (tcflush(STDIN_FILENO, TCIFLUSH) < 0) {
        fprintf(stderr, "tcflush error: %s\n", strerror(errno));
    }
    interrupt_line_toggle = 0;
    return kbdr_internal;
}

static uint16_t read_kbsr(void) {
    fd_set set;
    int result;
    struct timeval tv;
    if (READY_BIT(kbsr_internal)) {
        return kbsr_internal;
    }
    memset(&tv, 0, sizeof(tv));
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    result = select(STDIN_FILENO + 1, &set, NULL, NULL, &tv);
    if (result < 0) {
        fprintf(stderr, "select error: %s\n", strerror(errno));
    }
    if (result > 0) {
        kbsr_internal |= SET_READY_BIT;
        if (INTERRUPT_BIT(kbsr_internal)) {
            interrupt_line_toggle = 1;
        }
    }
    return kbsr_internal;
}

static int read_interrupt_line(uint8_t *vec_location, uint8_t *priority) {
    int ret = 0;
    read_kbsr();
    if (interrupt_line_toggle) {
        *vec_location = VEC_LOCATION;
        ret = 1;
    }
    interrupt_line_toggle = 0;
    return ret;
}

interrupt_line keyboard_get_interrupt_line(void) {
    return read_interrupt_line;
}

int keyboard_get_registers(struct io_register **kbdr, struct io_register **kbsr) {
    if (set_nonblock(STDIN_FILENO) < 0) {
        return -1;
    }
    *kbdr = &kbdr_external;
    *kbsr = &kbsr_external;
    return 0;
}






