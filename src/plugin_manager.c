#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "list.h"
#include "plugin_manager.h"
#include "simulator.h"
#include "device.h"

#define PATH_BUFSIZ BUFSIZ

static void create_func_null_error_string(char *error_string, const char *path) {
    static char func_not_found_error_str[] = "init_device_plugin is null in:";	
    snprintf(error_string, PM_ERROR_STR_SIZ, "%s%s\n", func_not_found_error_str, path);
}

static void cpy_error_string(char *dst, char *src) {
    snprintf(dst, PM_ERROR_STR_SIZ, "%s", src);
}

static int build_path(char *dst, const char *directory_str, const char *file_name, size_t dst_size) {
    int combined_size;
    if (directory_str[strlen(directory_str) - 1] != '/') {
        combined_size = snprintf(dst, dst_size, "%s%s%s", directory_str, "/", file_name);
    } else {
        combined_size = snprintf(dst, dst_size, "%s%s", directory_str, file_name);
    }
    if (combined_size < 0) {
        return -1;
    }
    if (combined_size >= dst_size) {
        return 0;
    }
    return 1;
}

static int check_extension(const char *path, const char *extension) {
    char *path_ext;
    path_ext = strrchr(path, '.');
    if (path_ext == NULL) {
        return 0;
    }
    path_ext += 1;
    return strcmp(path_ext, extension) == 0;
}

static struct device *init_plugin(void *dlhandle, const char *path, char *error_string) {
    struct device *(*init_device_plugin)(void) = NULL;
    struct device *plugin;
    char *dl_error_string;
    init_device_plugin = dlsym(dlhandle, "init_device_plugin");
    if (init_device_plugin == NULL) {
        dl_error_string = dlerror();
        if (dl_error_string == NULL) {
            create_func_null_error_string(error_string, dl_error_string);
        } else {
            cpy_error_string(error_string, dl_error_string);
        }
        return NULL;
    }
    plugin = init_device_plugin();
    if (plugin == NULL) {
        cpy_error_string(error_string, strerror(errno));
        return NULL;
    }
    return plugin;
}

static int load_device_plugin(const char *path, struct device_data *device_plugin, char *error_string) {
    device_plugin->dlhandle = dlopen(path, RTLD_NOW);
    if (device_plugin->dlhandle == NULL) {
        cpy_error_string(error_string, dlerror());
        return -1;
    }
    device_plugin->device = init_plugin(device_plugin->dlhandle, path, error_string);
    if (device_plugin->device == NULL) {
        dlclose(device_plugin->dlhandle);
        return -1;
    }
    return 0;
}

static int add_device_plugin(List *device_plugins, const char *path, char *error_string) {
    struct device_data device_data;
    if (load_device_plugin(path, &device_data, error_string) < 0) {
        goto load_device_plugin_err;
    }
    if (list_add(device_plugins, &device_data) < 0) {
        cpy_error_string(error_string, strerror(errno));
        goto list_add_err;
    }
    return 0;

list_add_err:
    device_data.device->free(device_data.device);
    dlclose(device_data.dlhandle);
load_device_plugin_err:
    return -1;        
}

void pm_plugins_free(List *device_plugins) {
    size_t num_plugins, i;
    num_plugins = list_num_elements(device_plugins);
    for (i = 0; i < num_plugins; ++i) {
        struct device_data *cur_plugin;
        cur_plugin = list_get(device_plugins, i);
        cur_plugin->device->free(cur_plugin->device);
        dlclose(cur_plugin->dlhandle);
    }
    list_free(device_plugins);
}

List *pm_load_device_plugins(const char *dir_path, const char *extension, char *error_string) {
    List *devices = NULL;
    DIR *dir;
    struct dirent *dp;
    dir = opendir(dir_path);
    if (dir == NULL) {
        goto err_str;
    }
    devices = list_new(sizeof(struct device_data), 2, 1.0);
    if (devices == NULL) {
        goto err_str;
    }
    errno = 0;
    while ((dp = readdir(dir)) != NULL) {
        char path[PATH_BUFSIZ];
        struct stat file_stat;
        int build_path_result;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }
        build_path_result = build_path(path, dir_path, dp->d_name, sizeof(path));
        if (build_path_result < 0) {
            goto err;
        }
        if (!build_path_result) {
            continue;
        }
        if (!check_extension(path, extension)) {
            continue;
        }
        if (stat(path, &file_stat) < 0) {
            goto err_str;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            continue;
        }
        if (add_device_plugin(devices, path, error_string) < 0) {	
            goto err;
        }
    }
    if (errno != 0) {
        goto err_str;
    }
    closedir(dir);
    return devices;

err_str:
    cpy_error_string(error_string, strerror(errno));
err:
    if (devices != NULL) pm_plugins_free(devices);
    if (dir != NULL) closedir(dir);    
    return NULL;
}