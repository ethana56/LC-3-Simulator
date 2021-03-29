#ifndef DEVICE_H
#define DEVICE_H

#include <stdlib.h>
#include <stdint.h>

struct host {
    void *data;
    void (*write_output)(struct host *, char);
    void (*alert_interrupt)(struct host *, uint8_t vec, uint8_t priority);
};

enum address_method {RANGE, SEPERATE};

struct device {
    void *data;
    uint16_t (*read_register)(struct device *, uint16_t address);
    void (*write_register)(struct device *, uint16_t address, uint16_t value);
    void (*on_input)(struct device *, char);
    void (*on_tick)(struct device *);
    void (*free)(struct device *);
    uint16_t *(*get_addresses)(struct device *, size_t *);
    enum address_method (*get_address_method)(struct device *);
};

#endif 