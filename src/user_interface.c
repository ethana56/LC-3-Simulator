#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "util.h"
#include "plugin_manager.h"
#include "terminal.h"
#include "simulator.h"
#include "list.h"
#include "device_io_impl.h"
#include "lc3_reg.h"

#ifdef __linux__
#define EXTENSION "so"
#endif
#ifdef __APPLE__
#define EXTENSION "dylib"
#endif

#define INITIAL_PROGRAM_BUF_LEN 50
#define MAX_COMMAND_STR_SIZE 100

#define UI_REG_MEM_MODE_INDEX         1
#define UI_MEM_WRITE_VALUE_INDEX      2
#define UI_MEM_TOKEN1_READ_INDEX      2
#define UI_MEM_TOKEN2_READ_INDEX      3
#define UI_MEM_TOKEN1_WRITE_INDEX     3
#define UI_MEM_TOKEN2_WRITE_INDEX     4

#define UI_REG_DST_INDEX 2
#define UI_REG_VAL_INDEX 3

#define UI_STEP_AMT_INDEX 1

#define UI_LOAD_FILENAME_INDEX 1

struct ui {
    Simulator *simulator;
    List *device_plugins;
    struct device_io *device_io_impl;
};

enum ui_status {CONTINUE, DONE, ERROR};
enum ui_reg_mem_mode {UI_REG_MEM_READ, UI_REG_MEM_WRITE};

typedef enum ui_status (*command_func)(struct ui *, List *);

struct command {
    char *command_str;
    command_func func;
};

static enum ui_status ui_step(struct ui *, List *);
static enum ui_status ui_help(struct ui *, List *);
static enum ui_status ui_run(struct ui *, List *);
static enum ui_status ui_mem(struct ui *, List *);
static enum ui_status ui_reg(struct ui *, List *);
static enum ui_status ui_load(struct ui *, List *);
static enum ui_status ui_quit(struct ui *, List *);

/* static const char *os_filename = "os.obj"; */

static const char *help_string = "help - print this message\n"
                                  "mem read [address], (optional)[address] - display all mem between the two addresses\n"
                                  "mem write [value] [address] (optional)[address] - write mem between the two address\n"
                                  "reg read - display registers\n"
                                  "reg write [value] [register] - write register\n"
                                  "run - execute LC-3 program to the end\n"
                                  "step [low] [high] - step LC-3 program between low and high\n"
                                  "load [file] - load lc3 program\n"
                                  "quit - close simulator\n";

static const struct command commands[] = {{"step", ui_step}, {"help", ui_help}, {"run", ui_run}, {"mem", ui_mem}, {"reg", ui_reg}, {"load", ui_load}, {"quit", ui_quit}}; 
static const int num_commands = 7;

static const char *REG_MEM_WRITE_MODE_STR = "write";
static const char *REG_MEM_READ_MODE_STR  = "read";

static int get_user_input(char *buffer) {
    printf("\ncommand> ");
    fflush(stdout);
    if (fgets(buffer, MAX_COMMAND_STR_SIZE, stdin) == NULL) {
        return -1;
    }
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

static int ui_get_token(List *tokens, size_t index, char **token) {
    void *item;
    item = list_get(tokens, index);
    if (item == NULL) {
        return 0;
    }
    *token = *(char **)item;
    return 1;
}

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

static void ui_load_print_usage(void) {
    printf("load usage: load [filename]\n");
}

static int ui_load_open_file(char *filename, FILE **program_file) {
    int status;
    status = 1;
    *program_file  = fopen(filename, "r");
    if (*program_file == NULL) {
        if (errno == ENOMEM) {
            perror(NULL);
            abort();
        }
        status = 0;
    }
    return status;
}

static enum ui_status ui_quit(struct ui *user_interface, List *input_tokens) {
    return DONE;
}

static enum ui_status ui_load(struct ui *user_interface, List *input_tokens) {
    FILE *program_file;
    int open_file_status;
    char *filename;
    if (!ui_get_token(input_tokens, UI_LOAD_FILENAME_INDEX, &filename)) {
        ui_load_print_usage();
        return CONTINUE;
    }
    open_file_status = ui_load_open_file(filename, &program_file);
    if (open_file_status < 0) {
        return ERROR;
    }
    if (!open_file_status) {
        printf("%s: %s\n", filename, strerror(errno));
        return CONTINUE;
    }
    if (simulator_load_program(user_interface->simulator, program_reader, program_file) < 0) {
        printf("%s: %s\n", filename, strerror(errno));
    }
    fclose(program_file);
    return CONTINUE;
}

static int ui_convert_address_token(char *token, uint16_t *address) {
    char *endptr;
    long long converted;
    converted = string_to_ll_10_or_16(token, &endptr);
    if (endptr == token || *endptr != '\0') {
        return 0;
    }
    if (converted < LOW_ADDRESS || converted > HGIH_ADDRESS) {
        return 0;
    }
    *address = converted;
    return 1;
}

static void ui_mem_print(struct ui *user_interface, uint16_t low, uint16_t high) {
    long cur_address;
    printf("%-13s%-13s\n", "address", "value");
    for (cur_address = low; cur_address <= high; ++cur_address) {
        uint16_t cur_value;
        enum simulator_address_status address_status;
        char cur_address_str[7], cur_value_str[7];
        address_status = simulator_read_address(user_interface->simulator, cur_address, &cur_value);
        snprintf(cur_address_str, sizeof(cur_address_str), "0X%04X", (uint16_t)cur_address);
        if (address_status == VALUE) {
            snprintf(cur_value_str, sizeof(cur_value_str), "0X%04X", cur_value);
            printf("%-13s%-13s\n", cur_address_str, cur_value_str);
        } else if (address_status == DEVICE_REGISTER) {
            printf("%-13s%-13s\n", cur_address_str, "DEVICE");
        } else if (address_status == OUT_OF_BOUNDS) {
            printf("%-13s%-13s\n", cur_address_str, "OUT OF BOUNDS");
        }
    }
}

static void ui_mem_write(struct ui *user_interface, uint16_t value, uint16_t low, uint16_t high) {
    long cur_address;
    for (cur_address = low; cur_address <= high; ++cur_address) {
        simulator_write_address(user_interface->simulator, cur_address, value);
    }
}

static int ui_reg_mem_convert_mode(char *mode_token, enum ui_reg_mem_mode *mode) {
    int result;
    result = 1;
    if (strcmp(mode_token, REG_MEM_READ_MODE_STR) == 0) {
        *mode = UI_REG_MEM_READ;
    } else if (strcmp(mode_token, REG_MEM_WRITE_MODE_STR) == 0) {
        *mode = UI_REG_MEM_WRITE;
    } else {
        result = 0;
    }
    return result;
}

static int ui_mem_get_address_tokens(List *input_tokens, enum ui_reg_mem_mode mode, char **low_token, char **high_token) {
    size_t low_index, high_index;
    if (mode == UI_REG_MEM_READ) {
        low_index = UI_MEM_TOKEN1_READ_INDEX;
        high_index = UI_MEM_TOKEN2_READ_INDEX;
    } else {
        low_index = UI_MEM_TOKEN1_WRITE_INDEX;
        high_index = UI_MEM_TOKEN2_WRITE_INDEX;
    }
    if (!ui_get_token(input_tokens, low_index, low_token)) {
        return 0;
    }
    if (!ui_get_token(input_tokens, high_index, high_token)) {
        *high_token = NULL;
    }
    return 1;
}

static int ui_mem_get_addresses(List *input_tokens, enum ui_reg_mem_mode mode, uint16_t *low, uint16_t *high) {
    char *low_token, *high_token;
    if (!ui_mem_get_address_tokens(input_tokens, mode, &low_token, &high_token)) {
        return 0;
    }
    if (!ui_convert_address_token(low_token, low)) {
        return 0;
    }
    if (high_token == NULL) {
        *high = *low;
    } else if (!ui_convert_address_token(high_token, high)) {
        return 0;
    }
    return 1;
}

/* inclusive */
static int ui_convert_str_range(char *str, long long *val, long long low, long long high) {
    char *endptr;
    *val = string_to_ll_10_or_16(str, &endptr);
    return (endptr != str && *endptr == '\0' && *val >= low && *val <= high);
}

static int ui_convert_str_to_uint16(char *val_token, uint16_t *val) {
    long long converted;
    if (!ui_convert_str_range(val_token, &converted, 0, UINT16_MAX)) {
        return 0;
    }
    *val = converted;
    return 1;
}

static int ui_mem_get_write_val(List *input_tokens, uint16_t *write_val) {
    char *val_token;
    return ui_get_token(input_tokens, UI_MEM_WRITE_VALUE_INDEX, &val_token) &&
           ui_convert_str_to_uint16(val_token, write_val); 
}

static int ui_mem_get_mode(List *input_tokens, enum ui_reg_mem_mode *mode) {
    char *mode_token;
    return ui_get_token(input_tokens, UI_REG_MEM_MODE_INDEX, &mode_token) &&
           ui_reg_mem_convert_mode(mode_token, mode); 
}

static void ui_mem_print_usage(void) {
    printf("mem usage: mem [mode] [low address] [high address] [write val]\n");
}

static enum ui_status ui_mem(struct ui *user_interface, List *input_tokens) {
    uint16_t low, high;
    enum ui_reg_mem_mode mode;
    if (!ui_mem_get_mode(input_tokens, &mode)) {
        goto err;
    }
    if (!ui_mem_get_addresses(input_tokens, mode, &low, &high)) {
        goto err;
    }
    if (mode == UI_REG_MEM_READ) {
        ui_mem_print(user_interface, low, high);
    } else if (mode == UI_REG_MEM_WRITE) {
        uint16_t write_val;
        if (!ui_mem_get_write_val(input_tokens, &write_val)) {
            goto err;
        }
        ui_mem_write(user_interface, write_val, low, high);
    }
    return CONTINUE;

err:
    ui_mem_print_usage();
    return CONTINUE;
}

static void ui_reg_print(struct ui *user_interface) {
    Simulator *simulator;
    uint16_t r0, r1, r2, r3, r4, r5, r6, r7, pc, psr, usp, ssp;
    simulator = user_interface->simulator;
    r0 =  simulator_read_register(simulator, REG_R0);
    r1 =  simulator_read_register(simulator, REG_R1);
    r2 =  simulator_read_register(simulator, REG_R2);
    r3 =  simulator_read_register(simulator, REG_R3);
    r4 =  simulator_read_register(simulator, REG_R4);
    r5 =  simulator_read_register(simulator, REG_R5);
    r6 =  simulator_read_register(simulator, REG_R6);
    r7 =  simulator_read_register(simulator, REG_R7);
    pc =  simulator_read_register(simulator, REG_PC);
    psr = simulator_read_register(simulator, REG_PSR);
    usp = simulator_read_register(simulator, REG_USP);
    ssp = simulator_read_register(simulator, REG_SSP);
    printf("R0: 0X%04X, R1: 0X%04X, R2: 0X%04X, R3: 0X%04X, R4: 0X%04X, R5: 0X%04X, R6: 0X%04X, R7: 0X%04X\n",
        r0, r1, r2, r3, r4, r5, r6, r7);
    printf("PC: 0X%04X, PSR: 0X%04X, USP: 0X%04X, SSP: 0X%04X\n", 
        pc, psr, usp, ssp);
}

static int ui_reg_get_dst(List *input_tokens, enum lc3_reg *reg_dst) {
    char *reg_token;
    return ui_get_token(input_tokens, UI_REG_DST_INDEX, &reg_token) &&
           lc3_reg_str_convert(reg_token, reg_dst);
}

static int ui_reg_get_val(List *input_tokens, uint16_t *val) {
    char *val_token;
    return ui_get_token(input_tokens, UI_REG_VAL_INDEX, &val_token) &&
           ui_convert_str_to_uint16(val_token, val); 
}

static void ui_reg_write(struct ui *user_interface, enum lc3_reg reg_dst, uint16_t val) {
    simulator_write_register(user_interface->simulator, reg_dst, val);
}

static int ui_reg_get_mode(List *input_tokens, enum ui_reg_mem_mode *mode) {
    char *mode_token;
    return ui_get_token(input_tokens, UI_REG_MEM_MODE_INDEX, &mode_token) &&
           ui_reg_mem_convert_mode(mode_token, mode);
}

static void ui_reg_print_usage(void) {
    printf("reg usage: reg [read/write] [register] [write value]\n");
}

static enum ui_status ui_reg(struct ui *user_interface, List *input_tokens) {
    enum ui_reg_mem_mode mode;
    if (!ui_reg_get_mode(input_tokens, &mode)) {
        goto err;
    }
    if (mode == UI_REG_MEM_READ) {
        ui_reg_print(user_interface);
    } else if (mode == UI_REG_MEM_WRITE) {
        enum lc3_reg reg_dst;
        uint16_t val;
        if (!ui_reg_get_val(input_tokens, &val)) {
            goto err;
        }
        if (!ui_reg_get_dst(input_tokens, &reg_dst)) {
            goto err;
        }
        ui_reg_write(user_interface, reg_dst, val);
    }
    return CONTINUE;

err:
    ui_reg_print_usage();
    return CONTINUE;    
}

static int ui_step_get_amt(List *input_tokens, long long *amt) {
    char *step_amt_token;
    if (!ui_get_token(input_tokens, UI_STEP_AMT_INDEX, &step_amt_token)) {
        *amt = 1;
        return 1;
    }
    return ui_convert_str_range(step_amt_token, amt, 0, LLONG_MAX);
}

static enum ui_status ui_step(struct ui *user_interface, List *input_tokens) {
    long long step_amt;
    int step_status;
    if (!ui_step_get_amt(input_tokens, &step_amt)) {
        return CONTINUE;
    }   
    step_status = simulator_step(user_interface->simulator, step_amt);
    return step_status < 0 ? ERROR : CONTINUE;
}

static void tokenize_input(char *input, List *tokens) {
    char *context;
    char *token;
    while ((token = strtok_r(input, " ,\n", &context)) != NULL) {
        list_add(tokens, &token);
        input = NULL;
    }
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
    command = *(char **)list_get(input_tokens, 0);
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
    input_tokens = list_new(sizeof(char *), 5, 1, &util_list_allocator);
    for (;;) {
        enum ui_status status;
        if (get_user_input(input) < 0) {
            goto err;
        }
        tokenize_input(input, input_tokens);
        status = execute_command(user_interface, input_tokens);
        if (status == DONE) {
            break;
        }
        if (status == ERROR) {
            return -1;
        }
        list_clear(input_tokens);
    }
    list_free(input_tokens);
    return 0;

err:
    list_free(input_tokens);
    return -1;    
}

static void attach_devices(struct ui *user_interface) {
    List *devices;
    size_t i, num_devices;
    devices = user_interface->device_plugins;
    num_devices = list_num_elements(devices);
    for (i = 0; i < num_devices; ++i) {
        struct device_data *data;
        data = list_get(devices, i);
        if (simulator_attach_device(user_interface->simulator, data->device) < 0) {
            fprintf(stderr, "%s: address map conficts with another device.\n", data->path);
            continue;
        }
    }
}

static void on_load_plugin_error(const char *path, const char *error_string, enum pm_error error_type, void *data) {
    switch (error_type) {
    case PM_ERROR_OPENDIR:
        fprintf(stderr, "Error opening plugin directiry %s: %s.\n", path, error_string);
        break;
    case PM_ERROR_PLUGIN_LOAD:
        fprintf(stderr, "Error loading device plugin %s: %s.\n", path, error_string);
        break;        
    }
}

void ui_free_plugins(List *device_plugins) {
    size_t num_plugins, i;
    num_plugins = list_num_elements(device_plugins);
    for (i = 0; i < num_plugins; ++i) {
        struct device_data *cur_plugin;
        cur_plugin = list_get(device_plugins, i);
        pm_free_plugin(cur_plugin);
    }
    list_free(device_plugins);
}

int start(void) {
    struct ui user_interface;
    char const *plugin_dirs[] = {"../obj"};
    pm_set_on_error(on_load_plugin_error, NULL);
    user_interface.device_plugins = pm_load_device_plugins(plugin_dirs, 1, EXTENSION);
    user_interface.device_io_impl = create_device_io_impl(STDIN_FILENO, STDOUT_FILENO);
    user_interface.simulator = simulator_new(user_interface.device_io_impl);
    attach_devices(&user_interface);
    if (ui_loop(&user_interface) < 0) {
        perror(NULL);
    }
    free_io_impl(user_interface.device_io_impl);
    ui_free_plugins(user_interface.device_plugins);
    simulator_free(user_interface.simulator);
    return 0;
}