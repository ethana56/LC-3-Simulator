#ifndef DEVICE_IO_H
#define DEVICE_IO_H

struct device_io {
    void *data;
    int (*get_char)(struct device_io *, char *);
    int (*write_char)(struct device_io *, char);
    int (*start)(struct device_io *);
    int (*end)(struct device_io *);
};

#endif