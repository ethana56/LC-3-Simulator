#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"
#include "terminal.h"
#include "device_io.h"

struct device_io_impl_data {
    int infd;
    int outfd;
};

static int io_impl_get_char(struct device_io *io, char *c) {
    struct device_io_impl_data *data;
    ssize_t result;
    data = io->data;
    result = read(data->infd, c, 1);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
    }
    return result;
}

static int io_impl_write_char(struct device_io *io, char c) {
    struct device_io_impl_data *data;
    ssize_t result;
    data = io->data;
    while ((result = write(data->outfd, &c, 1)) != 1) {
        if (result < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
            break;
        } 
    }
    return result;
}

static int io_impl_start(struct device_io *io) {
    struct device_io_impl_data *data;
    int infd_old_status, outfd_old_status;
    data = io->data;
    infd_old_status = set_nonblock(data->infd);
    if (infd_old_status < 0) {
        goto set_infd_err;
    }
    outfd_old_status = set_nonblock(data->outfd);
    if (outfd_old_status < 0) {
        goto set_outfd_err;
    }
    if (init_terminal() < 0) {
        goto init_terminal_err;
    }
    return 0;

init_terminal_err:
    fcntl(data->outfd, F_SETFL, outfd_old_status);
set_outfd_err:
    fcntl(data->infd, F_SETFL, infd_old_status);
set_infd_err:
    return -1;            
}

static int io_impl_end(struct device_io *io) {
    struct device_io_impl_data *data;
    int infd_old_status, outfd_old_status;
    data = io->data;
    infd_old_status = set_blocking(data->infd);
    if (infd_old_status < 0) {
        goto set_infd_err;
    }
    outfd_old_status = set_blocking(data->outfd);
    if (outfd_old_status < 0) {
        goto set_outfd_err;
    }
    reset_terminal();
    /* TODO: Reset terminal */
    return 0;
set_outfd_err:
    fcntl(data->infd, F_SETFL, infd_old_status); 
set_infd_err:
    return -1;       
}

static void impl_init_device_io(struct device_io *io, struct device_io_impl_data *data) {
    io->data = data;
    io->end = io_impl_end;
    io->start = io_impl_start;
    io->get_char = io_impl_get_char;
    io->write_char = io_impl_write_char;
}

struct device_io *create_device_io_impl(int infd, int outfd) {
    struct device_io *io;
    struct device_io_impl_data *data;
    io = safe_malloc(sizeof(struct device_io));
    data = safe_malloc(sizeof(struct device_io_impl_data));
    data->infd = infd;
    data->outfd = outfd;
    impl_init_device_io(io, data);
    return io;
}

void free_io_impl(struct device_io *io) {
    free(io->data);
    free(io);
}


