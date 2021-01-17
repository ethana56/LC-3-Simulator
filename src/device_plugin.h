#ifndef DEVICE_PLUGIN_H
#define DEVICE_PLUGIN_H

#include <stdint.h>

struct host {
    void *data;
    int (*get_keyboard_in_fd)(struct host *);
    int (*get_display_out_fd)(struct host *);
    void (*add_listener_read)(struct host *, int, void (*)(int, void *), void *);
    void (*log_error)(struct host *, char *);
    void (*close)(struct host *);
};

struct device_plugin {
    void *data;	
    uint16_t *readable;
    uint16_t *writeable;
    uint16_t *readable_writeable;
    size_t num_readable;
    size_t num_writeable;
    size_t num_readable_writeable;
    uint16_t (*read_register)(uint16_t);
    void (*write_register)(uint16_t, uint16_t);
    void (*start)(struct host *);
    void (*cleanup)(void);
};

#endif
