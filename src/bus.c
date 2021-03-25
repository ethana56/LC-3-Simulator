#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>

#include "bus.h"

#define ATTACHMENT_SIZE_INIT       5
#define ATTACHMENT_SIZE_MULTIPLIER 2

struct interval {
    uint16_t low;
    uint16_t high;
};

struct bus_attachment {
    uint16_t (*read)(uint16_t);
    void (*write)(uint16_t, uint16_t);
    struct interval range;
};

struct mem {
    uint16_t value;
    char attachment_flag;
};

struct bus_impl {
    struct bus_attachment *attachments;
    size_t num_attachments;
    size_t attachments_size;
    struct mem memory[BUS_NUM_ADDRESSES];
};

Bus *bus_new(void) {
    Bus *bus;
    bus = malloc(sizeof(Bus));
    if (bus == NULL) {
        return NULL;
    }
    bus->attachments = malloc(sizeof(struct bus_attachment) * ATTACHMENT_SIZE_INIT);
    if (bus->attachments == NULL) {
        free(bus);
        return NULL;
    }
    bus->num_attachments = 0;
    bus->attachments_size = ATTACHMENT_SIZE_INIT;
    memset(bus->memory, 0, sizeof(struct mem) * BUS_NUM_ADDRESSES);
    return bus;
}

void bus_free(Bus *bus) {
    free(bus->attachments);
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
    if (point >= range->low && point <= range->high) {
        result = 0;
    } else if (point < range->low) {
        result = -1;
    } else if (point > range->high) {
        result = 1;
    }
    return result;
} 

static int bus_grow_attachments(Bus *bus) {
    struct bus_attachment *bigger_attachments;
    size_t bigger_num_members;
    bigger_num_members = bus->attachments_size * ATTACHMENT_SIZE_MULTIPLIER;
    bigger_attachments = realloc(bus->attachments, sizeof(struct bus_attachment) * bigger_num_members);
    if (bigger_attachments == NULL) {
        return -1;
    }
    bus->attachments = bigger_attachments;
    bus->attachments_size = bigger_num_members;
    return 0;
}

/* only call if addresses don't overlap with any existing */
static int bus_add_attachment(Bus *bus, uint16_t (*read)(uint16_t), void (*write)(uint16_t, uint16_t), struct interval *interval) {
    struct bus_attachment *attachment;
    int i;
    if (bus->num_attachments == bus->attachments_size) {
        if (bus_grow_attachments(bus) < 0) {
            return -1;
        }
    }
    attachment = &bus->attachments[bus->num_attachments++];
    attachment->range = *interval;
    attachment->read = read;
    attachment->write = write;
    qsort(bus->attachments, bus->num_attachments, sizeof(struct bus_attachment), attachment_comparator);
    for (i = interval->low; i <= interval->high; ++i) {
        bus->memory[i].attachment_flag = 1;
    }
    return 0;
}

static int intervals_overlap(struct interval *interval1, struct interval *interval2) {
    return (interval2->low <= interval1->high) && (interval2->high >= interval1->low);
}

static int bus_contains_interval(Bus *bus, struct interval *interval) {
    size_t i;
    for (i = 0; i < bus->num_attachments; ++i) {
        struct bus_attachment *cur_attachment;
        cur_attachment = &bus->attachments[i];
        if (intervals_overlap(&cur_attachment->range, interval)) {
            return 1;
        }
    }
    return 0;
}

int bus_attach(Bus *bus, uint16_t (*read)(uint16_t), void (*write)(uint16_t, uint16_t), uint16_t low, uint16_t high) {
    struct interval interval;
    interval.high = high;
    interval.low = low;
    if (interval.high >= BUS_NUM_ADDRESSES || bus_contains_interval(bus, &interval)) {
        errno = EINVAL;
        return -1;
    }
    if (bus_add_attachment(bus, read, write, &interval) < 0) {
        return -1;
    }
    return 0;
}

static struct bus_attachment *bus_point_search(Bus *bus, uint16_t point) {
    return bsearch(&point, bus->attachments, bus->num_attachments, sizeof(struct bus_attachment), bsearch_attachment_comparator);
}

void bus_print(Bus *bus) {
    size_t i;
    for (i = 0; i < bus->num_attachments; ++i) {
        printf("LOW: %u, HIGH: %u\n", bus->attachments[i].range.low, bus->attachments[i].range.high);
    }
}

uint16_t bus_read_memory(Bus *bus, uint16_t address) {
    return bus->memory[address].value;
}

uint16_t bus_read(Bus *bus, uint16_t address) {
    struct mem *mem_val;
    mem_val = &bus->memory[address];
    if (mem_val->attachment_flag) {
        struct bus_attachment *attachment;
        /* Will never be null */
        attachment = bus_point_search(bus, address);
        mem_val->value = attachment->read(address);
    }
    return mem_val->value;
}

void bus_write(Bus *bus, uint16_t address, uint16_t value) {
    struct mem *mem_val;
    mem_val = &bus->memory[address];
    mem_val->value = value;
    if (mem_val->attachment_flag) {
        struct bus_attachment *attachment;
        attachment = bus_point_search(bus, address);
        attachment->write(address, mem_val->value);
    }
}