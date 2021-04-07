#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <stdio.h>

#include "cpu.h"
#include "interrupt_controller.h"
#include "bus.h"
#include "list.h"
#include "simulator.h"
#include "device.h"
#include "device_io.h"

struct simulator {
    struct bus_accessor bus_accessor;
    struct interrupt_checker inter_check;
    struct host host;
    Cpu *cpu;
    Bus *bus;
    InterruptController *inter_cont;
    struct device_io *device_io;
    List *on_input_devices;
    List *on_tick_devices;
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

static void simulator_update_devices_input(Simulator *simulator, char input) {
    size_t i, num_on_input_devices;
    List *on_input_devices;
    on_input_devices = simulator->on_input_devices;
    num_on_input_devices = list_num_elements(on_input_devices);
    for (i = 0; i < num_on_input_devices; ++i) {
        struct device *cur_device;
        cur_device = *(struct device **)list_get(on_input_devices, i);
        cur_device->on_input(cur_device, input);
    }
}

static void simulator_check_input(Simulator *simulator) {
    char input;
    if (simulator->device_io->get_char(simulator->device_io, &input)) {
        simulator_update_devices_input(simulator, input);
    }
}

static void simulator_update_devices_on_tick(Simulator *simulator) {
    size_t i, num_on_tick_devices;
    List *on_tick_devices;
    if (simulator->on_tick_devices == NULL) return;
    on_tick_devices = simulator->on_tick_devices;
    num_on_tick_devices = list_num_elements(on_tick_devices);
    for (i = 0; i < num_on_tick_devices; ++i) {
        struct device *cur_device;
        cur_device = list_get(on_tick_devices, i);
        cur_device->on_tick(cur_device);
    }
}

static void simulator_on_tick(void *data) {
    Simulator *simulator;
    simulator = data;
    simulator_check_input(simulator);
    simulator_update_devices_on_tick(simulator);
}

enum simulator_address_status simulator_read_address(Simulator *simulator, uint16_t address, uint16_t *value) {
    if (address < LOW_ADDRESS || address > HGIH_ADDRESS) {
        return OUT_OF_BOUNDS;
    }
    if (bus_is_device_register(simulator->bus, address)) {
        return DEVICE_REGISTER;
    }
    *value = bus_read_memory(simulator->bus, address);
    return VALUE;
}

int simulator_run_until_end(Simulator *simulator) {
    if (simulator->device_io->start(simulator->device_io) < 0) {
        return -1;
    }
    cpu_execute_until_end(simulator->cpu);
    if (simulator->device_io->end(simulator->device_io) < 0) {
        return -1;
    }
    return 0;
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

static int simulator_add_on_input_subscription(Simulator *simulator, struct device *device) {
    if (simulator->on_input_devices == NULL) {
        simulator->on_input_devices = list_new(sizeof(struct device *), 2, 2.0);
        if (simulator->on_input_devices == NULL) {
            return -1;
        }
    }
    if (list_add(simulator->on_input_devices, &device) < 0) {
        return -1;
    }
    return 0;
}

static int simulator_add_on_tick_subscription(Simulator *simulator, struct device *device) {
    if (simulator->on_tick_devices == NULL) {
        simulator->on_tick_devices = list_new(sizeof(struct device *), 1, 2.0);
        if (simulator->on_tick_devices == NULL) {
            return -1;
        }
    }
    if (list_add(simulator->on_tick_devices, &device) < 0) {
        return -1;
    }
    return 0;
}

static void simulator_remove_on_input_subscription(Simulator *simulator, struct device *device) {

}

/*static void simulator_remove_on_tick_subscription(Simulator *simulator, struct device *device) {

}*/

static int simulator_check_device_subscriptions(Simulator *simulator, struct device *device) {
    if (device->on_input != NULL) {
        if (simulator_add_on_input_subscription(simulator, device) < 0) {
            goto add_on_input_subscription_err;
        }
    }
    if (device->on_tick != NULL) {
        if (simulator_add_on_tick_subscription(simulator, device) < 0) {
            goto add_on_tick_subscription_err;
        }
    }
    return 0;

add_on_tick_subscription_err:
    simulator_remove_on_input_subscription(simulator, device);
add_on_input_subscription_err:
    return -1;        
}

int simulator_attach_device(Simulator *simulator, struct device *device) {
    if (bus_attach(simulator->bus, device) < 0) {
        return -1;
    }
    device->start(device, &simulator->host);
    device->start(device, &simulator->host);
    return simulator_check_device_subscriptions(simulator, device);
}

static void simulator_host_write_output(struct host *host, char output) {
    Simulator *simulator;
    simulator = host->data;
    simulator->device_io->write_char(simulator->device_io, output);
}

static void simulator_host_alert_interrupt(struct host *host, uint8_t vec, uint8_t priority) {
    Simulator *simulator;
    simulator = host->data;
    interrupt_controller_alert(simulator->inter_cont, vec, priority);
}

static void init_host(Simulator *simulator) {
    simulator->host.data = simulator;
    simulator->host.write_output = simulator_host_write_output;
    simulator->host.alert_interrupt = simulator_host_alert_interrupt;
}

Simulator *simulator_new(struct device_io *device_io) {
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
    init_host(simulator);
    simulator->device_io = device_io;
    simulator->on_input_devices = NULL;
    simulator->on_tick_devices = NULL;
    cpu_subscribe_on_tick(simulator->cpu, simulator_on_tick, simulator);
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
    list_free(simulator->on_input_devices);
    list_free(simulator->on_tick_devices);
    free(simulator);
}