#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "cpu.h"
#include "interrupt_controller.h"
#include "bus.h"
#include "list.h"
#include "simulator.h"
#include "device.h"

struct simulator {
    struct bus_accessor bus_accessor;
    struct interrupt_checker inter_check;
    Cpu *cpu;
    Bus *bus;
    InterruptController *inter_cont;
    struct device_io *device_io;
    struct device **on_input_devices;
    size_t num_on_input;
    struct device **on_tick_devices;
    size_t num_on_tick;
};

static int simulator_check_interrupt(
    struct interrupt_checker *inter_check, 
    uint8_t cmp_priority, 
    uint8_t *vec, 
    uint8_t *priority, 
    int (*comparator)(uint8_t, uint8_t)
) {
    return interrupt_controller_check((InterruptController *)inter_check->data, cmp_priority, vec, priority, comparator);
}

static void init_interrupt_checker(InterruptController *inter_cont, struct interrupt_checker *inter_check) {
    inter_check->data = inter_cont;
    inter_check->check_interrupt = simulator_check_interrupt;
}

static uint16_t simulator_bus_read(struct bus_accessor *bus_access, uint16_t address) {
    return bus_read((Bus *)bus_access->data, address);
}

static void simulator_bus_write(struct bus_accessor *bus_access, uint16_t address, uint16_t value) {
    bus_write((Bus *)bus_access->data, address, value);
}

static void init_bus_accessor(Bus *bus, struct bus_accessor *bus_access) {
    bus_access->data = bus;
    bus_access->read = simulator_bus_read;
    bus_access->write = simulator_bus_write;
}

int simulator_bind_addresses(Simulator *simulator, uint16_t *addresses, size_t num_addresses, uint16_t (*addr_read)(uint16_t), void (*addr_write)(uint16_t, uint16_t)) {
    size_t i;
    for (i = 0; i < num_addresses; ++i) {
        if (bus_attach(simulator->bus, addr_read, addr_write, addresses[i], addresses[i]) < 0) {
            /* TODO: Revert to defined state */
            return -1;
        }
    }
    return 0;
}

int simulator_bind_addresses_range(Simulator *simulator, uint16_t low, uint16_t high, uint16_t (*addr_read)(uint16_t), void (*addr_write)(uint16_t, uint16_t)) {
    return bus_attach(simulator->bus, addr_read, addr_write, low, high);
}

void simulator_unbind_all_addresses(Simulator *simulator) {

}

void simulator_subscribe_on_tick(Simulator *simulator, void (*callback)(void *), void *data) {
    cpu_subscribe_on_tick(simulator->cpu, callback, data);
}

void simulator_run_until_end(Simulator *simulator) {
    simulator->device_io->start(simulator->device_io);
    cpu_execute_until_end(simulator->cpu);
    simulator->device_io->end(simulator->device_io);
}

int simulator_load_program(Simulator *simulator, int (*callback)(void *, uint16_t *), void *data) {
    int callback_result;
    uint16_t cur_address, starting_address;
    uint16_t cur_word;
    callback_result = callback(data, &starting_address);
    if (callback_result < 1) {
        return callback_result;
    }
    cur_address = starting_address;
    while ((callback_result = callback(data, &cur_word)) > 0) {
        bus_write(simulator->bus, cur_address, cur_word);
        ++cur_address;
    }
    cpu_set_program_counter(simulator->cpu, starting_address);
    return callback_result;
}

static int simulator_check_device_subscriptions(Simulator *simulator, struct device **devices, size_t num_devices) {
    List *on_input_devices, *on_tick_devices;
    size_t i;
    on_input_devices = new_List(2, 1.0);
    if (on_input_devices == NULL) {
        goto on_input_devices_alloc_err;
    }
    on_tick_devices = new_List(2, 1.0);
    if (on_tick_devices == NULL) {
        goto on_tick_devices_alloc_err;
    }
    for (i = 0; i < num_devices; ++i) {
        struct device *cur_device;
        cur_device = devices[i];
        if (cur_device->on_input != NULL) {
            if (list_add(on_input_devices, cur_device) < 0) {
                goto list_add_err;
            }
        }
        if (cur_device->on_tick != NULL) {
            if (list_add(on_tick_devices, cur_device) < 0) {
                goto list_add_err;
            }
        }
    }
    simulator->on_input_devices = (struct device **)list_free_and_return_as_array(on_input_devices, &simulator->num_on_input);
    simulator->on_tick_devices = (struct device **)list_free_and_return_as_array(on_tick_devices, &simulator->num_on_tick);
    return 0;

list_add_err:
    list_free(on_tick_devices);
on_tick_devices_alloc_err:
    list_free(on_input_devices);
on_input_devices_alloc_err:
    return -1;            
}

int simulator_attach_device(Simulator *simulator, struct device *device) {
    if (bus_attach(simulator->bus, device) < 0) {
        return -1;
    }
    return simulator_check_device_subscriptions(simulator, device);
}

Simulator *simulator_new(struct device_input *device_io) {
    Simulator *simulator;
    simulator = malloc(sizeof(Simulator));
    if (simulator == NULL) {
        goto simulator_alloc_err;
    }
    simulator->bus = bus_new();
    if (simulator->bus == NULL) {
        goto bus_alloc_err;
    }
    simulator->inter_cont = interrupt_controller_new();
    if (simulator->inter_cont == NULL) {
        goto inter_cont_alloc_err;
    }
    init_bus_accessor(simulator->bus, &simulator->bus_accessor);
    init_interrupt_checker(simulator->inter_cont, &simulator->inter_check);
    simulator->cpu = new_Cpu(&simulator->bus_accessor, &simulator->inter_check);
    if (simulator->cpu == NULL) {
        goto cpu_alloc_err;
    }
    simulator->device_io = device_io;
    return simulator;

cpu_alloc_err:
    interrupt_controller_free(simulator->inter_cont);
inter_cont_alloc_err:
    bus_free(simulator->bus);
bus_alloc_err:
    free(simulator);
simulator_alloc_err:
    return NULL;            
}

void simulator_free(Simulator *simulator) {
    bus_free(simulator->bus);
    interrupt_controller_free(simulator->inter_cont);
    free_cpu(simulator->cpu);
    free(simulator);
}