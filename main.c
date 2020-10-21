#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "cpu.h"
#include "io.h"
#include "keyboard.h"
#include "display.h"

static struct io_device **get_devices(int *);

int main(int argc, char **argv) {
    Cpu *cpu;
    struct io_device **io_devices;
    int num_devices;
    if (argc != 2) {
        fprintf(stderr, "Incorrect args\n");
        exit(EXIT_FAILURE);
    }
    cpu = new_Cpu(argv[1]);
    io_devices = get_devices(&num_devices);
    if (io_devices == NULL) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    cpu_attach_devices(cpu, io_devices, num_devices);
    cpu_execute_until_end(cpu);
    return 0;
}

static struct io_device **get_devices(int *num_devices) {
    struct io_device **devices;
    struct io_device *cur_device;
    int devices_index = 0;
    devices = malloc(sizeof(sizeof(struct io_device *) * 4));
    if (devices == NULL) {
        return NULL;
    }
    cur_device = keyboard_get_device();
    if (cur_device == NULL) {
        free(devices);
        return NULL;
    }
    devices[devices_index++] = cur_device;
    cur_device = display_get_device();
    if (cur_device == NULL) {
        free(devices[devices_index - 1]);
        free(devices);
        return NULL;
    }
    devices[devices_index++] = cur_device;
    *num_devices = devices_index;
    return devices;
}
