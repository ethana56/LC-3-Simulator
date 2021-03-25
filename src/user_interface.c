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
#include "input_output.h"
#include "plugin_manager.h"
#include "terminal.h"
#include "simulator.h"
#include "list.h"

#ifdef __linux__
#define EXTENSION "so"
#endif
#ifdef __APPLE_
#define EXTENSION "dylib"
#endif

#define INITIAL_PROGRAM_BUF_LEN 50

struct ui {
    Simulator *simulator;
    struct device_plugin_data **plugins, **on_input_plugins, **on_tick_plugins;
    size_t num_plugins, num_on_input_plugins, num_on_tick_plugins;
    struct host host;
};


enum command_option {RUN, INVALID};

static int get_user_input(char *buffer, size_t buffer_size) {
    size_t str_len;
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

static int prepare_terminal(void) {
    if (set_nonblock(STDIN_FILENO) < 0 || set_nonblock(STDOUT_FILENO)) {
        /* TODO: REVERT STDIN_FILENO TO BLOCKING IF SET_NONBLOCK(STDOUT_FILENO) FAILS */
        return -1;
    }
    if (init_terminal() < 0) {
        return -1;
    }
    return 0;
}

static int ui_loop(struct ui *user_interface) {
    char input[100];
    for (;;) {
        if (get_user_input(input, sizeof(input)) < 0) return -1;
        if (strcmp(input, "run") == 0) {
            if (prepare_terminal() < 0) {
                return -1;
            }
            simulator_run_until_end(user_interface->simulator);
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

static int attach_plugins(struct ui *user_interface) {
    size_t i;
    int status = 0;
    for (i = 0; i < user_interface->num_plugins && status >= 0; ++i) {
        struct device_plugin *cur_plugin;
        uint16_t *addresses;
        size_t num_addresses;
        uint16_t (*read_register)(uint16_t);
        void (*write_register)(uint16_t, uint16_t);
        cur_plugin = user_interface->plugins[i]->plugin;
        addresses = cur_plugin->addresses;
        num_addresses = cur_plugin->num_addresses;
        read_register = cur_plugin->read_register;
        write_register = cur_plugin->write_register;
        
        switch (cur_plugin->method) {
        case RANGE:
            status = simulator_bind_addresses_range(user_interface->simulator, addresses[0], addresses[1], read_register, write_register);
            break;
        case SEPERATE:
            status = simulator_bind_addresses(user_interface->simulator, addresses, num_addresses, read_register, write_register);        
        }
    }
    if (status < 0) {
        simulator_unbind_all_addresses(user_interface->simulator);
    }
    return status;
}

static void on_tick(void *data) {
    struct ui *user_interface;
    ssize_t result;
    size_t i;
    char input;
    user_interface = data;
    result = read(STDIN_FILENO, &input, 1);
    if (result < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return;
        }
        fprintf(stderr, "error reading input\n");
    }
    for (i = 0; i < user_interface->num_on_input_plugins; ++i) {
        user_interface->on_input_plugins[i]->plugin->on_input(input);
    }
    for (i = 0; i < user_interface->num_on_tick_plugins; ++i) {
        user_interface->on_tick_plugins[i]->plugin->on_tick();
    }
}

static void write_output(struct host *host, char output) {
    safe_write(STDOUT_FILENO, &output, 1);
}

static void alert_interrupt(struct host *host, uint8_t vec, uint8_t priority) {

}

static void subscribe_simulator_events(struct ui *user_interface) {
    Simulator *simulator;
    simulator = user_interface->simulator;
    simulator_subscribe_on_tick(simulator, on_tick, user_interface);
}

static int check_plugin_subscriptions(struct ui *user_interface) {
    List *on_input_plugins, *on_tick_plugins;
    size_t i;
    on_input_plugins = new_List(2, 1.0);
    if (on_input_plugins == NULL) {
        goto on_input_plugins_alloc_err;
    }
    on_tick_plugins = new_List(2, 1.0);
    if (on_tick_plugins == NULL) {
        goto on_tick_plugins_alloc_err;
    }
    for (i = 0; i < user_interface->num_plugins; ++i) {
        struct device_plugin_data *device_plugin;
        device_plugin = user_interface->plugins[i];
        if (device_plugin->plugin->on_input != NULL) {
            if (list_add(on_input_plugins, device_plugin) < 0) {
                goto list_add_err;
            }
        }
        if (device_plugin->plugin->on_tick != NULL) {
            if (list_add(on_tick_plugins, device_plugin) < 0) {
                goto list_add_err;
            }
        }
    }
    user_interface->on_input_plugins = (struct device_plugin_data **)list_free_and_return_as_array(on_input_plugins, &user_interface->num_on_input_plugins);
    user_interface->on_tick_plugins = (struct device_plugin_data **)list_free_and_return_as_array(on_tick_plugins, &user_interface->num_on_tick_plugins);
    return 0;

list_add_err:
    list_free(on_tick_plugins);
on_tick_plugins_alloc_err:
    list_free(on_input_plugins);
on_input_plugins_alloc_err:
    return -1;            
}

void start(void) {
    struct ui user_interface;
    char load_devices_err_str[PM_ERROR_STR_SIZ];

    user_interface.simulator = simulator_new();
    if (user_interface.simulator == NULL) {
        err_exit(strerror(errno));
    }
    user_interface.host.data = &user_interface;
    user_interface.host.write_output = write_output;
    user_interface.host.alert_interrupt = alert_interrupt;
    user_interface.plugins = pm_load_device_plugins("../devices", EXTENSION, &user_interface.num_plugins, &user_interface.host, load_devices_err_str);
    if (user_interface.plugins == NULL) {
        simulator_free(user_interface.simulator);
        err_exit(load_devices_err_str);
    }
    if (attach_plugins(&user_interface) < 0) {
        simulator_free(user_interface.simulator);
        pm_plugins_free(user_interface.plugins, user_interface.num_plugins);
        err_exit(strerror(errno));
    }
    if (check_plugin_subscriptions(&user_interface) < 0) {
        simulator_free(user_interface.simulator);
        pm_plugins_free(user_interface.plugins, user_interface.num_plugins);
        err_exit(strerror(errno));
    }
    subscribe_simulator_events(&user_interface);
    if (ui_loop(&user_interface) < 0) {
        simulator_free(user_interface.simulator);
        pm_plugins_free(user_interface.plugins, user_interface.num_plugins);
        err_exit(strerror(errno));
    }
}








