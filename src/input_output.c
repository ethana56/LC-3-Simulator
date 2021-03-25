#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>

#include "input_output.h"
#include "util.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

struct std_input_output {
    int keyboard_pipe[2];
    int display_pipe[2];
    int mode_pipe[2];
    pthread_mutex_t stdio_usage_mutex;
};

static int switch_to_user_io_mode(StdInputOutput *std_input_output) {
    return 0;
}

static int get_display_read_fd(StdInputOutput *std_input_output) {
    return std_input_output->display_pipe[PIPE_READ];
}

static int get_keyboard_write_fd(StdInputOutput *std_input_output) {
    return std_input_output->keyboard_pipe[PIPE_WRITE];
}

static int get_mode_read_fd(StdInputOutput *std_input_output) {
    return std_input_output->mode_pipe[PIPE_READ];
}

static int open_communication(int *fds, int non_block_index) {
    if (pipe(fds) < 0) {
        return -1;
    }
    /* DON"T SET TO NONBLOCKING MODE */
    if (set_nonblock(fds[non_block_index]) < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    return 0;
}

static void close_communication(int *fds) {
    close(fds[0]);
    close(fds[1]);
}

static int initialize(StdInputOutput *io) {
    if (open_communication(io->keyboard_pipe, PIPE_WRITE) < 0) {
        return -1;
    }
    if (open_communication(io->display_pipe, PIPE_READ) < 0) {
        close_communication(io->keyboard_pipe);
        return -1;
    }
    if (pipe(io->mode_pipe) < 0) {
        close_communication(io->keyboard_pipe);
        close_communication(io->display_pipe);
        return -1;
    }
    return 0;
}

static int get_largest_fd(int fd1, int fd2, int fd3) {
    int largest = fd1;
    if (fd2 > largest) {
        largest = fd2;
    }
    if (fd3 > largest) {
        largest = fd3;
    }
    return largest;
}

/* This function only deals with one byte at a time from the 
 * keyboard because I am not sure if writing multiple bytes 
 * to a non blocking pipe works. May need to be re-writen.
 */
static int update_keyboard(StdInputOutput *std_input_output) {
    char input;
    ssize_t read_result = read(STDIN_FILENO, &input, 1);
    if (read_result < 0) {
        return -1;
    }
    ssize_t write_result = write(get_keyboard_write_fd(std_input_output), &input, 1);
    if (write_result < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
        return -1;
    }
    return 0;
}

static int update_display(StdInputOutput *std_input_output) {
    unsigned char buff[BUFSIZ];
    ssize_t read_result = read(get_display_read_fd(std_input_output), buff, sizeof(buff));
    if (read_result < 0) {
        return -1;
    }
    ssize_t write_result = safe_write(STDOUT_FILENO, buff, read_result);
    if (write_result != read_result) {
        return -1;
    }
    return 0;
}

static void *listener(void *arg) {
    StdInputOutput *std_input_output = arg;
    pthread_mutex_lock(&std_input_output->stdio_usage_mutex);

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(std_input_output->display_pipe[PIPE_READ], &fdset);
    FD_SET(std_input_output->mode_pipe[PIPE_READ], &fdset);

    int largest_fd = get_largest_fd(STDIN_FILENO, std_input_output->display_pipe[PIPE_READ], 
                                    std_input_output->mode_pipe[PIPE_READ]);

    for (;;) {
        fd_set temp = fdset;
        int select_result = select(largest_fd + 1, &fdset, NULL, NULL, NULL);
        if (select_result < 0 && errno == EINTR) {
            continue;
        }
        if (FD_ISSET(STDIN_FILENO, &fdset)) {
            update_keyboard(std_input_output);
        }
        if (FD_ISSET(get_display_read_fd(std_input_output), &fdset)) {
            update_display(std_input_output);
        }
        if (FD_ISSET(get_mode_read_fd(std_input_output), &fdset)) {
            switch_to_user_io_mode(std_input_output);
        }
        fdset = temp;
    }           
    return NULL;            
}

static int start_listener(StdInputOutput *std_input_output) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, listener, std_input_output) < 0) {
        return -1;
    }
    return 0;
}

int std_input_output_get_display_fd(StdInputOutput *std_input_output) {

}

int std_input_output_get_keyboard_fd(StdInputOutput *std_input_output) {

}

void give_back_std_input_output(StdInputOutput *std_input_output) {
    
}

StdInputOutput *grab_std_input_output(int *err) {
    static StdInputOutput std_input_output;
    static int initialized = 0;
    *err = 0;
    if (initialized) {
        return NULL;
    }
    if (initialize(&std_input_output) < 0) {
        *err = 1;
        return NULL;
    }
    initialized = 1;
    if (start_listener(&std_input_output) < 0) {
        *err = 1;
        return NULL;
    }
    return &std_input_output;
}
