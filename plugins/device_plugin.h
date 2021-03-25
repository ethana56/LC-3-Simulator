#ifndef DEVICE_PLUGIN_H
#define DEVICE_PLUGIN_H

#include <stdint.h>
#include <stdlib.h>

enum address_method {RANGE, SEPERATE};

struct host {
    void *data;
    void (*write_output)(struct host *, char);
    void (*alert_interrupt)(struct host *, uint8_t vec, uint8_t priority);
};

struct device_plugin {
    uint16_t (*read_register)(uint16_t address);
    void (*write_register)(uint16_t address, uint16_t value);
    void (*on_input)(char);
    void (*on_tick)(void);
    void (*free)(struct device_plugin *);
    uint16_t *addresses;
    size_t num_addresses;
    enum address_method method;
};

#endif
