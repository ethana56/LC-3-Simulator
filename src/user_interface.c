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
#define MAX_COMMAND_STR_SIZE 100

struct ui {
    Simulator *simulator;
    List *device_plugins;
    struct device_io *device_io_impl;
};

enum ui_status {CONTINUE, DONE, ERROR};

typedef enum ui_status (*command_func)(struct ui *, List *);

struct command {
    char *command_str;
    command_func func;
};

static enum ui_status ui_help(struct ui *, List *);
static enum ui_status ui_run(struct ui *, List *);
static enum ui_status ui_mem(struct ui *, List *);

/* static const char *os_filename = "os.obj"; */

static const char *help_string = "help - print this message\nrun all = run entire program\n"
                                  "read mem [address], [address] - display all mem between the two addresses\n"
                                  "read mem [address] - read mem at address\n"
                                  "write mem [address] - write mem at address";

static const struct command commands[] = {{"help", ui_help}, {"run", ui_run}, {"mem", ui_mem}}; 
static const int num_commands = 3;                                

enum command_option {RUN, INVALID};

static int get_user_input(char *buffer) {
    printf("command>");
    fflush(stdout);
    if (fgets(buffer, MAX_COMMAND_STR_SIZE, stdin) == NULL) {
        return -1;
    }
    return 0;
}

/*static int program_reader(void *data, uint16_t *program_word) {
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
}*/

/* static int load_program(struct ui *user_interface, char *program_name) {
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
} */

static enum ui_status ui_help(struct ui *user_interface, List *input_tokens) {
    printf("%s\n", help_string);
    return CONTINUE;
}

static enum ui_status ui_run(struct ui *user_interface, List *input_tokens) {
    if (list_num_elements(input_tokens) == 1) {
        if (simulator_run_until_end(user_interface->simulator) < 0) {
            return ERROR;
        }
    }
    return CONTINUE;
}

static enum ui_status ui_mem(struct ui *user_interface, List *input_tokens) {
    return 0;
}

static int tokenize_input(char *input, List *tokens) {
    char *context;
    char *token;
    while ((token = strtok_r(input, " ,\n", &context)) != NULL) {
        if (list_add(tokens, token) < 0) {
            return -1;
        }
        input = NULL;
    }
    return 0;
}

static command_func get_command_func(char const *command) {
    command_func func;
    int i;
    func = NULL;
    for (i = 0; i < num_commands; ++i) {
        struct command const *cur_command;
        cur_command = &commands[i];
        if (strcmp(cur_command->command_str, command) == 0) {
            func = cur_command->func;
            break;
        }
    }
    return func;
}

static enum ui_status execute_command(struct ui *user_interface, List *input_tokens) {
    char const *command;
    command_func func;
    if (list_num_elements(input_tokens) == 0) {
        return CONTINUE;
    }
    command = *(char ** const)list_get(input_tokens, 0);
    func = get_command_func(command);
    if (func == NULL) {
        printf("%s: invalid command\n", command);
        return CONTINUE;
    }
    return func(user_interface, input_tokens);
}

static int ui_loop(struct ui *user_interface) {
    char input[MAX_COMMAND_STR_SIZE + 1];
    List *input_tokens;
    input_tokens = list_new(sizeof(char *), 5, 1);
    if (input_tokens == NULL) {
        return -1;
    }
    for (;;) {
        enum ui_status status;
        if (get_user_input(input) < 0) {
            goto err;
        }
        if (tokenize_input(input, input_tokens) < 0) {
            goto err;
        }
        status = execute_command(user_interface, input_tokens);
        if (status == DONE) {
            break;
        }
        if (status == ERROR) {
            return -1;
        }
    }
    list_free(input_tokens);
    return 0;

err:
    list_free(input_tokens);
    return -1;    
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


int start(char *executable_path) {
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
    return 0;
}








