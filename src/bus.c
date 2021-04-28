#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>

#include "bus.h"
#include "device.h"
#include "list.h"
#include "util.h"

#define ATTACHMENT_SIZE_INIT       5
#define ATTACHMENT_SIZE_MULTIPLIER 2

struct interval {
    uint16_t low;
    uint16_t high;
};

struct bus_attachment {
    struct device *device;
    struct interval range;
};

struct mem {
    uint16_t value;
    char attachment_flag;
};

struct bus_impl {
    List *attachments;
    struct mem memory[BUS_NUM_ADDRESSES];
};

Bus *bus_new(void) {
    Bus *bus;
    bus = safe_malloc(sizeof(Bus));
    bus->attachments = list_new(sizeof(struct bus_attachment), ATTACHMENT_SIZE_INIT, ATTACHMENT_SIZE_MULTIPLIER, &util_list_allocator);
    memset(bus->memory, 0, sizeof(struct mem) * BUS_NUM_ADDRESSES);
    return bus;
}

void bus_free(Bus *bus) {
    list_free(bus->attachments);
    free(bus);
}

static int attachment_comparator(const void *first, const void *second) {
    const struct bus_attachment *first_attachment, *second_attachment;
    first_attachment = first;
    second_attachment = second;
    return first_attachment->range.low - second_attachment->range.low;
}

static int bsearch_attachment_comparator(const void *key, const void *element) {
    const struct bus_attachment *attachment;
    const struct interval *range;
    int result;
    uint16_t point;
    point = *(const uint16_t *)key;
    attachment = element;
    range = &attachment->range;
    result = 0;
    if (point < range->low) {
        result = -1;
    } else if (point > range->high) {
        result = 1;
    }  
    return result;
}

static int intervals_overlap(struct interval interval1, struct interval interval2) {
    return (interval2.low <= interval1.high) && (interval2.high >= interval1.low);
}

static int bus_contains_interval(Bus *bus, struct interval interval) {
    int i, num_attachments;
    int status = 0;
    num_attachments = list_num_elements(bus->attachments);
    for (i = 0; i < num_attachments; ++i) {
        struct bus_attachment *cur_attachment;
        cur_attachment = list_get(bus->attachments, i);
        if (cur_attachment->range.low > interval.high) {
            break;
        }
        if (intervals_overlap(cur_attachment->range, interval)) {
            status = 1;
            break;
        }
    }
    return status;
}

static int bus_add_attachment(Bus *bus, struct device *device, struct interval interval) {
    struct bus_attachment attachment;
    int i;
    if (bus_contains_interval(bus, interval)) {
        errno = EINVAL;
        return -1;
    }
    attachment.range = interval;
    attachment.device = device;
    list_add(bus->attachments, &attachment);
    list_sort(bus->attachments, attachment_comparator);
    for (i = interval.low; i <= interval.high; ++i) {
        bus->memory[i].attachment_flag = 1;
    }
    return 0;

}

static int bus_add_attachment_seperate(Bus *bus, struct device *device) {
    const uint16_t *addresses;
    size_t num_addresses, i;
    addresses = device->get_addresses(device, &num_addresses);
    for (i = 0; i < num_addresses; ++i) {
        struct interval interval;
        interval.low = addresses[i];
        interval.high = addresses[i];
        if (bus_add_attachment(bus, device, interval) < 0) {
            return -1;
        }
    }
    return 0;
}

static int bus_add_attachment_range(Bus *bus, struct device *device) {
    const uint16_t *addresses;
    size_t num_addresses;
    struct interval interval;
    addresses = device->get_addresses(device, &num_addresses);
    interval.low = addresses[0];
    interval.high = addresses[1];
    return bus_add_attachment(bus, device, interval);
}

int bus_attach(Bus *bus, struct device *device) {
    int status = -1;
    switch (device->get_address_method(device)) {
    case RANGE:
        status = bus_add_attachment_range(bus, device);
        break;
    case SEPERATE:
        status = bus_add_attachment_seperate(bus, device);
        break;        
    }
    return status;
}

static struct bus_attachment *bus_search(Bus *bus, uint16_t address) {
    return list_bsearch(bus->attachments, &address, bsearch_attachment_comparator);
}

void bus_print(Bus *bus) {
    size_t num_attachments, i;
    num_attachments = list_num_elements(bus->attachments);
    for (i = 0; i < num_attachments; ++i) {
        struct bus_attachment *cur_attachment;
        cur_attachment = list_get(bus->attachments, i);
        printf("LOW: %u, HIGH: %u\n", cur_attachment->range.low, cur_attachment->range.high);
    }

}

int bus_is_device_register(Bus *bus, uint16_t address) {
    return bus->memory[address].attachment_flag;
}

uint16_t bus_read_memory(Bus *bus, uint16_t address) {
    return bus->memory[address].value;
}

uint16_t bus_read(Bus *bus, uint16_t address) {
    struct mem *mem_val;
    uint16_t value;
    mem_val = &bus->memory[address];
    if (mem_val->attachment_flag) {
        struct bus_attachment *attachment;
        attachment = bus_search(bus, address);
        value = attachment->device->read_register(attachment->device, address);
    } else {
        value = mem_val->value;
    }
    return value;
}

void bus_write(Bus *bus, uint16_t address, uint16_t value) {
    struct mem *mem_val;
    mem_val = &bus->memory[address];
    if (mem_val->attachment_flag) {
        struct bus_attachment *attachment;
        attachment = bus_search(bus, address);
        attachment->device->write_register(attachment->device, address, value);
    } else {
        mem_val->value = value;
    }
}