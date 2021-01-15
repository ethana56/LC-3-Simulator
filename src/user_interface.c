#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>

#include "util.h"
#include "list.h"
#include "input_output.h"
#include "plugin_manager.h"
#include "device_impl.h"
#include "terminal.h"
#include "cpu.h"

enum command_option {RUN, INVALID};

/*static enum command_option get_command_option(char *command) {
    if (strcmp(command, "run") == 0) {
        return RUN;
    }
    return INVALID;
}*/

static int get_user_input(char *buffer, size_t buffer_size) {
    printf("command> ");
    fflush(stdout);
    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return -1;
    }
    return 0;
}

/*static int do_command(Cpu *simulator, char *buffer, int *err) {
    *err     = 0;
    int result = 0;
    enum command_option command = get_command_option(buffer);
    if (command == INVALID) {
        puts("Invalid command");
        result =  1;
    } else if (command == RUN) {
        cpu_execute_until_end(simulator);
        result = 0;
    }
    return result;
}*/

static int load_program(Cpu *cpu, char *path) {
    FILE *program;
    size_t amt_read;
    int result = 0;
    uint16_t buf[MAX_PROGRAM_LEN];
    program = fopen(path, "r");
    if (program == NULL) {
	perror(path);    
        return -1;
    }
    amt_read = read_convert_16bits(buf, MAX_PROGRAM_LEN, program);
    if (ferror(program)) {
	    perror(path);    
        result = -1;
    }
    fclose(program);
    cpu_load_program(cpu, buf, amt_read);
    return result;
}

static int load_operating_system(Cpu *cpu) {
    return load_program(cpu, "os.obj");
}

static int ui_loop(Cpu *cpu) {
    char buffer[BUFSIZ];
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strlen(buffer) - 1] = '\0';
    if (init_terminal() < 0) {
        return -1;
    }
    if (load_operating_system(cpu) < 0) {
        return -1;
    }
    if (load_program(cpu, buffer) < 0) {
        return -1;
    }
    if (set_nonblock(STDIN_FILENO) < 0) {
        return -1;
    }
    if (set_nonblock(STDOUT_FILENO) < 0) {
        return -1;
    }
    cpu_execute_until_end(cpu);
    return 0;
}

//static int ui_loop(Cpu *cpu, int infd, int outfd) {
    //char buffer[PATH_MAX];
    //int should_continue = 1;
    //set_user_io_mode(std_input_output);
    /*while (should_continue) {
        if (get_user_input(buffer, sizeof(buffer)) < 0) {
            return -1;
        }
        int err;
        should_continue = do_command(simulator, buffer, &err);
        if (err) {
            return -1;
        }

    }*/
    /*get_user_input(buffer, sizeof(buffer));
    simulator_load_program(simulator, buffer);
   simulator_execute_until_end(simulator);*/
    //return 0;
//}

static void log_error(char *error) {
    fputs(error, stderr);
    fputs("\n", stderr);
}

/*static int cmp_file_extension(const char *filename, const char *file_extension) {
    char *ext;
    ext = strchr(filename, '.');
    if (ext == NULL) {
        return 0;
    }
    ext = ext + 1;
    return strcmp(ext, file_extension) == 0;
}*/

static void free_devices(struct device **devices, size_t num_devices) {
    size_t i;
    for (i = 0; i < num_devices; ++i) {
        device_impl_free(devices[i]);
    }
    free(devices);
}

static void free_plugins(struct device_plugin **plugins, size_t num_plugins) {
    size_t i;
    for (i = 0; i < num_plugins; ++i) {
       pm_free_plugin(plugins[i]);
       plugins[i] = NULL;
    }
}

static struct device **create_devices(struct device_plugin **plugins, size_t num_plugins) {
    struct device **devices;
    size_t i;
    devices = malloc(sizeof(struct device *) * num_plugins);
    if (devices == NULL) {
        return NULL;
    }
    for (i = 0; i < num_plugins; ++i) {
        devices[i] = create_device_impl(plugins[i]);
	    if (devices[i] == NULL) {
            free_devices(devices, i);
	        free(devices);
	        return NULL;
	    }
    }
    return devices;
}

void start(void) {
    Cpu *cpu;
    struct device_plugin **plugins;
    struct device **devices;
    size_t num_devices;
    char error_string[PM_ERROR_STR_SIZ];
    plugins = pm_load_device_plugins("./devices", &num_devices, error_string);
    if (plugins == NULL) {
	    fprintf(stderr, "error opening devices directory: %s\n", error_string);    
	    goto load_plugins_err;
    }
    init_device_impl(STDIN_FILENO, STDOUT_FILENO, log_error);
    devices = create_devices(plugins, num_devices);
    if (devices == NULL) {
        perror(NULL);
	    goto create_devices_err;
    }
    /* Freeing the array of pointers, not the plugins themselved */
    free(plugins);
    plugins = NULL;
    cpu = new_Cpu();
    if (cpu == NULL) {
        perror(NULL);
	    goto create_cpu_err;
    }
    if (cpu_attach_devices(cpu, devices, num_devices) < 0) {
        perror(NULL);
	    goto cpu_attach_devices_err;
    }
    ui_loop(cpu);

cpu_attach_devices_err:
    free_cpu(cpu);
create_cpu_err:
    free_devices(devices, num_devices);
create_devices_err:
    free_plugins(plugins, num_devices);
    free(plugins);
load_plugins_err:
    exit(EXIT_FAILURE);

}










