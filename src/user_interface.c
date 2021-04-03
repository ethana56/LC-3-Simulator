#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "util.h"
#include "plugin_manager.h"
#include "terminal.h"
#include "simulator.h"
#include "list.h"
#include "device_io_impl.h"

#ifdef __linux__
#define EXTENSION "so"
#endif
#ifdef __APPLE__
#define EXTENSION "dylib"
#endif

#define INITIAL_PROGRAM_BUF_LEN 50

struct ui {
    Simulator *simulator;
    List *device_plugins;
    struct device_io *device_io_impl;
};

enum command_option {RUN, INVALID};

static int get_user_input(char *buffer, size_t buffer_size) {
    printf("command> ");
    fflush(stdout);
    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return -1;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
    return 0;
}

static int program_reader(void *data, uint16_t *program_word) {
    FILE *program_file = data;
    int result;
    result = fread(program_word, sizeof(uint16_t), 1, program_file);
    if (result == 0 && ferror(program_file)) {
        return -1;
    } else if (result == 0) {
        return 0;
    }
    
    *program_word = ntohs(*program_word);
    return result;
}

static int load_program(struct ui *user_interface, char *program_name) {
    FILE *program;
    program = fopen(program_name, "r");
    if (program == NULL) {
        return -1;
    }
    if (simulator_load_program(user_interface->simulator, program_reader, program) < 0) {
        return -1;
    }
    fclose(program);
    return 0;
}

static int ui_loop(struct ui *user_interface) {
    char input[100];
    for (;;) {
        if (get_user_input(input, sizeof(input)) < 0) return -1;
        if (strcmp(input, "run") == 0) {
            if (simulator_run_until_end(user_interface->simulator) < 0) {
                return -1;
            }
            return 0;
        }
        if (load_program(user_interface, input) < 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "%s: %s\n", input, strerror(errno));
                continue;
            }
            return -1;
        }
    }
}

static void err_exit(char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
    exit(EXIT_FAILURE);
}

static int attach_devices(struct ui *user_interface) {
    List *devices;
    size_t i, num_devices;
    devices = user_interface->device_plugins;
    num_devices = list_num_elements(devices);
    for (i = 0; i < num_devices; ++i) {
        struct device_data *data;
        data = list_get(devices, i);
        if (simulator_attach_device(user_interface->simulator, data->device) < 0) {
            if (errno == EINVAL) {
                fprintf(stderr, "(device plugin path here) address map conficts with another\n");
                continue;
            }
            return -1;
        }
    }
    return 0;
}


void start(void) {
    struct ui user_interface;
    char load_devices_err_str[PM_ERROR_STR_SIZ];

    user_interface.device_plugins = pm_load_device_plugins("./devices", EXTENSION, load_devices_err_str);
    if (user_interface.device_plugins == NULL) {

        err_exit(load_devices_err_str);
    }
    user_interface.device_io_impl = create_device_io_impl(STDIN_FILENO, STDOUT_FILENO);
    if (user_interface.device_io_impl == NULL) {
        pm_plugins_free(user_interface.device_plugins);
        err_exit(strerror(errno));
    }
    user_interface.simulator = simulator_new(user_interface.device_io_impl);
    if (user_interface.simulator == NULL) {
        free_io_impl(user_interface.device_io_impl);
        pm_plugins_free(user_interface.device_plugins);
        err_exit(strerror(errno));
    }
    if (attach_devices(&user_interface) < 0) {
        simulator_free(user_interface.simulator);
        pm_plugins_free(user_interface.device_plugins);
        free_io_impl(user_interface.device_io_impl);
        err_exit(strerror(errno));
    }
    if (ui_loop(&user_interface) < 0) {
        perror("ui loop");
        exit(EXIT_FAILURE);
    }
    free_io_impl(user_interface.device_io_impl);
    pm_plugins_free(user_interface.device_plugins);
    simulator_free(user_interface.simulator);
}








