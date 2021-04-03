#include <stdlib.h>
#include <stdint.h>

#include "device.h"

#define READY_BIT_SET_ON 0x8000
#define READY_BIT_SET_OFF 0x7FFF

#define DSR 0xFE04
#define DDR 0xFE06

#define INIT_DSR 0x8000
#define INIT_DDR 0

static const uint16_t display_addresses[] = {DSR, DDR};
static const size_t display_num_addresses = 2;
static const enum address_method display_method = SEPERATE;

struct display_data {
    struct host *host;
    uint16_t dsr;
    uint16_t ddr;
};

static uint16_t display_read_register(struct device *display_device, uint16_t address) {
    struct display_data *display_data;
    uint16_t value;
    display_data = display_device->data;
    switch (address) {
    case DSR:
        value = display_data->dsr;
        break;
    case DDR:
        value = display_data->ddr;
        break;    
    }
    return value;
}

static void display_write_register(struct device *display_device, uint16_t address, uint16_t value) {
    struct display_data *display_data;
    uint16_t dsr_ready_bit;
    display_data = display_device->data;
    switch (address) {
    case DSR:
        dsr_ready_bit = display_data->dsr & READY_BIT_SET_ON;
        display_data->dsr = value;
        display_data->dsr |= dsr_ready_bit;
        break;
    case DDR:
        display_data->host->write_output(display_data->host, value);
        break;       
    }
}

static void display_free(struct device *display_device) {
    free(display_device->data);
    free(display_device);
}

static void display_start(struct device *display_device, struct host *host) {
    struct display_data *display_data;
    display_data = display_device->data;
    display_data->host = host;
}

static const uint16_t *display_get_addresses(struct device *display_device, size_t *num_addresses) {
    *num_addresses = display_num_addresses;
    return display_addresses;
}

static enum address_method display_get_address_method(struct device *display_device) {
    return display_method;
}

static void init_display_device(struct device *display_device, struct display_data *data) {
    data->host = NULL;
    data->ddr = INIT_DDR;
    data->dsr = INIT_DSR;
    display_device->data = data;
    display_device->read_register = display_read_register;
    display_device->write_register = display_write_register;
    display_device->on_input = NULL;
    display_device->on_tick = NULL;
    display_device->start = display_start;
    display_device->get_addresses = display_get_addresses;
    display_device->get_address_method = display_get_address_method;
    display_device->free = display_free;
}

struct device *init_device_plugin(void) {
    struct device *display_device;
    struct display_data *data;
    display_device = malloc(sizeof(struct device));
    if (display_device == NULL) {
        return NULL;
    }
    data = malloc(sizeof(struct display_data));
    if (data == NULL) {
        free(display_device);
        return NULL;
    }
    init_display_device(display_device, data);
    return display_device;
}
