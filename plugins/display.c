#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "device_plugin.h"

#define DISPLAY_NUM_READABLE 0
#define DISPLAY_NUM_WRITEABLE 1
#define DISPLAY_NUM_READABLE_WRITEABLE 1

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0x7FFF

#define DSR 0xFE04
#define DDR 0xFE06

static uint16_t display_reg_writeable[] = {DDR};
static uint16_t display_reg_readable_writeable[] = {DSR};

static uint16_t dsr = READY_BIT_SET_ON;

static int display_out_fd;
static int waiting = 0;

static struct host *display_host_interface;

#define DISPLAY_REG_READABLE NULL
#define DISPLAY_REG_WRITEABLE display_reg_writeable
#define DISPLAY_REG_READABLE_WRITEABLE display_reg_readable_writeable

static void display_write_register(uint16_t address, uint16_t value) {
    /* Can only be ddr */
    ssize_t amt;
    unsigned char val;
    val = value;
    while ((amt = write(display_out_fd, &val, 1)) <= 0) {
        if (amt < 0 && errno == EINTR) {
            continue;
	}
    }
    if (amt < 0) {
        display_host_interface->log_error(display_host_interface, strerror(errno));
    }
}

static uint16_t display_read_register(uint16_t address) {
    /* Can only be dsr */
    return dsr;	
}

static void display_start(struct host *host_interface) {
    display_host_interface = host_interface;
    display_out_fd = host_interface->get_display_out_fd(host_interface);
    if (display_out_fd < 0) {
        host_interface->log_error(host_interface, "Display plugin cannot get display out fd");
        host_interface->close(host_interface);
        return;
    }
}

static void display_cleanup(void) {
 
}

int init_device_plugin(struct device_plugin *plugin) {
    plugin->readable = DISPLAY_REG_READABLE;
    plugin->writeable = DISPLAY_REG_WRITEABLE;
    plugin->readable_writeable = DISPLAY_REG_READABLE_WRITEABLE;
    plugin->num_readable = DISPLAY_NUM_READABLE;
    plugin->num_writeable = DISPLAY_NUM_WRITEABLE;
    plugin->num_readable_writeable = DISPLAY_NUM_READABLE_WRITEABLE;
    plugin->read_register = display_read_register;
    plugin->write_register = display_write_register;
    plugin->start = display_start;
    plugin->cleanup = display_cleanup;
    return 0;
}
