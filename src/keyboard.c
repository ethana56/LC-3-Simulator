#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "device_plugin.h"

#define KEYBOARD_NUM_READABLE 1
#define KEYBOARD_NUM_WRITEABLE 0
#define KEYBOARD_NUM_READABLE_WRITEABLE 1

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0X7FFF

#define KBSR 0xFE00
#define KBDR 0xFE02

static uint16_t keyboard_reg_readable[] = {KBDR};
static uint16_t keyboard_reg_readable_writeable[] = {KBSR};

static uint16_t kbsr = 0;
static uint16_t kbdr = 0;

static struct host *keyboard_host_interface;

static int keyboard_in_fd;

#define KEYBOARD_REG_READABLE keyboard_reg_readable
#define KEYBOARD_REG_WRITEABLE NULL
#define KEYBOARD_REG_READABLE_WRITEABLE keyboard_reg_readable_writeable

static void keyboard_write_register(uint16_t address, uint16_t value) {
    /* Address can only be KBSR */
    kbsr = value;	
}

static uint16_t keyboard_read_register(uint16_t address) {
    uint16_t result;
    switch (address) {
    case KBSR:
	    result = kbsr;
	    break;
    case KBDR:
	    result = kbdr;
	    kbsr &= READY_BIT_SET_OFF;
	    break;
    default:
	    /* This will never happen */
	    result = 0;
    }
    return result;
}

static void keyboard_update(int fd, void *data) {
    unsigned char input;
    ssize_t amt_read;
    amt_read = read(fd, &input, sizeof(input));
    if (amt_read < 0 && errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
    }
    if (amt_read < 0) {
        keyboard_host_interface->log_error(keyboard_host_interface, strerror(errno));
	    keyboard_host_interface->close(keyboard_host_interface);
	    return;
    }
    /* Make sure last data has been read */
    if (kbsr == 0) {
        kbdr = input;
	    kbsr |= READY_BIT_SET_ON;
    }
}

static void keyboard_cleanup(void) {
    /* RETURN KEYBOARD_IN_FD */
}

static void keyboard_start(struct host *host_interface) {
    int keyboard_in_fd;
    keyboard_host_interface = host_interface;
    keyboard_in_fd = host_interface->get_keyboard_in_fd(host_interface);
    if (keyboard_in_fd < 0) {
        host_interface->log_error(host_interface, "Keyboard plugin cannot get in fd");
        host_interface->close(host_interface);
        return;
    }
    host_interface->add_listener_read(host_interface, keyboard_in_fd, keyboard_update, NULL);
}

int init_device_plugin(struct device_plugin *plugin) {
    plugin->readable = KEYBOARD_REG_READABLE;
    plugin->writeable = KEYBOARD_REG_WRITEABLE;
    plugin->readable_writeable = KEYBOARD_REG_READABLE_WRITEABLE;
    plugin->num_readable = KEYBOARD_NUM_READABLE;
    plugin->num_writeable = KEYBOARD_NUM_WRITEABLE;
    plugin->num_readable_writeable = KEYBOARD_NUM_READABLE_WRITEABLE;
    plugin->read_register = keyboard_read_register;
    plugin->write_register = keyboard_write_register;
    plugin->start = keyboard_start;
    plugin->cleanup = keyboard_cleanup;
    return 0;
}
