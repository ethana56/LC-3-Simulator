#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <dlfcn.h>


#include "device_event_loop.h"
#include "cpu.h"
#include "device_plugin.h"
#include "device_impl.h"

/* It is ok to have static buffers here because there is no reason why this module would ever be called from multiple threads */
static char *last_error_string = NULL;

struct device_data {
    struct host *host_interface;
    struct device_plugin *plugin;
    Dvel *dvel;
};

static struct file_descriptor_data {
    int keyboard_fd;
    int keyboard_fd_taken;
    int display_fd;
    int display_fd_taken;
} fd_data = {-1, 0, -1, 0};
static pthread_mutex_t fd_data_lock = PTHREAD_MUTEX_INITIALIZER;

static void (*external_log_error)(char *) = NULL;
static pthread_mutex_t log_error_lock = PTHREAD_MUTEX_INITIALIZER;

void init_device_impl(int keyboard_fd, int display_fd, void (*log_error)(char *)) {
    fd_data.keyboard_fd = keyboard_fd;
    fd_data.display_fd = display_fd;
    external_log_error = log_error;
}

static void internal_log_error(char *msg) {
    if (external_log_error != NULL) {
        pthread_mutex_lock(&log_error_lock);
        external_log_error(msg);
        pthread_mutex_unlock(&log_error_lock);
    }	
}

static void host_log_error(struct host *host_interface, char *msg) {
    internal_log_error(msg);	
}

static int get_keyboard_in_fd(struct host *host_interface) {
    int keyboard_fd;	
    pthread_mutex_lock(&fd_data_lock);
    if (fd_data.keyboard_fd_taken) return -1;
    fd_data.keyboard_fd_taken = 1;
    keyboard_fd = fd_data.keyboard_fd;
    pthread_mutex_unlock(&fd_data_lock);
    return keyboard_fd;
}

static int get_display_out_fd(struct host *host_interface) {
    int display_fd;
    pthread_mutex_lock(&fd_data_lock);
    if (fd_data.display_fd_taken) return -1;
    fd_data.display_fd_taken = 1;
    display_fd = fd_data.display_fd;
    pthread_mutex_unlock(&fd_data_lock);
    return display_fd;
}

static void close_device(struct device_data *dev_data) {

}

static void host_add_listener_read(struct host *host_interface, int fd, void (*cb)(int, void *), void *data) {
    struct device_data *dev_data = host_interface->data;
    //return;
    if (dvel_add_listener_read(dev_data->dvel, fd, cb, data) < 0) {
        close_device(dev_data);
    }
}

static uint16_t device_read_register(struct device *device, uint16_t address) {
    struct device_data *data;
    uint16_t value;
    data = device->data;
    //printf("About to lock\n");
    dvel_lock(data->dvel);
    value = data->plugin->read_register(address);
    dvel_unlock(data->dvel);
    //printf("unlocking\n");
    return value;
}

static void device_write_register(struct device *device, uint16_t address, uint16_t value) {
    struct device_data *data;
    data = device->data;
    dvel_lock(data->dvel);
    data->plugin->write_register(address, value);
    dvel_unlock(data->dvel);
}

void cleanup(struct device *device) {

}

void close(struct host *host_interface) {

}

static void init_host_interface(struct host *host_interface, struct device_data *dev_data) {
    host_interface->data = dev_data;
    host_interface->get_keyboard_in_fd = get_keyboard_in_fd;
    host_interface->get_display_out_fd = get_display_out_fd;
    host_interface->add_listener_read = host_add_listener_read;
    host_interface->log_error = host_log_error;
    host_interface->close = close;
}

static void init_device(struct device *device, struct device_data *dev_data) {
    device->data = dev_data;
    device->readable = dev_data->plugin->readable;
    device->writeable = dev_data->plugin->writeable;
    device->readable_writeable = dev_data->plugin->readable_writeable;

    device->num_readable = dev_data->plugin->num_readable;
    device->num_writeable = dev_data->plugin->num_writeable;
    device->num_readable_writeable = dev_data->plugin->num_readable_writeable;

    device->read_register = device_read_register;
    device->write_register = device_write_register;
}

static struct device_data *create_dev_data(struct device_plugin *plugin) {
    struct device_data *dev_data;
    dev_data = malloc(sizeof(struct device_data));
    if (dev_data == NULL) {
        return NULL;
    }
    dev_data->host_interface = malloc(sizeof(struct host));
    if (dev_data->host_interface == NULL) {
        free(dev_data);
        return NULL;
    }
    init_host_interface(dev_data->host_interface, dev_data);
    dev_data->plugin = plugin;
    dev_data->dvel = dvel_new();
    if (dev_data->dvel == NULL) {
        free(dev_data->host_interface);
        free(dev_data);
        return NULL;
    }
    return dev_data;
}

struct device *create_device_impl(struct device_plugin *plugin) {
    struct device_data *dev_data;
    struct device *device;
    device = malloc(sizeof(struct device));
    if (device == NULL) {
        return NULL;
    }
    dev_data = create_dev_data(plugin);
    if (dev_data == NULL) {
        free(device);
        return NULL;
    }
    init_device(device, dev_data);
    dev_data->plugin->start(dev_data->host_interface);
    return device;
}

struct device_plugin *device_impl_free(struct device *device) {
    struct device_data *dev_data;	
    struct device_plugin *plugin;
    dvel_free(dev_data->dvel);
    plugin = dev_data->plugin;
    free(dev_data);
    free(device);
    return plugin;
}
