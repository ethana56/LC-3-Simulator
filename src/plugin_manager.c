#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <libgen.h>

#include "list.h"
#include "plugin_manager.h"
#include "simulator.h"
#include "device.h"
#include "util.h"

struct plugin_names {
    char *path;
    char *name;
};

static const char *func_null_error_string = "init_device_plugin is null";

static void (*pm_on_error_ptr)(const char *path, const char *error_string, enum pm_error error_type, void *data) = NULL;
static void *pm_on_error_data;

static void pm_on_error(const char *path, const char *error_string, enum pm_error error_type) {
    if (pm_on_error_ptr != NULL) {
        pm_on_error_ptr(path, error_string, error_type, pm_on_error_data);
    }
}

static int build_path(char *dst, const char *directory_str, const char *file_name, size_t dst_size) {
    int combined_size;
    if (directory_str[strlen(directory_str) - 1] != '/') {
        combined_size = snprintf(dst, dst_size, "%s%s%s", directory_str, "/", file_name);
    } else {
        combined_size = snprintf(dst, dst_size, "%s%s", directory_str, file_name);
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

static struct device *pm_init_plugin(void *dlhandle, const char *path) {
    struct device *(*init_device_plugin)(void);
    struct device *plugin;
    char *dl_error_string;
    init_device_plugin = dlsym(dlhandle, "init_device_plugin");
    if (init_device_plugin == NULL) {
        dl_error_string = dlerror();
        if (dl_error_string == NULL) {
            pm_on_error(path, func_null_error_string, PM_ERROR_PLUGIN_LOAD);
        } else {
            pm_on_error(path, strerror(errno), PM_ERROR_PLUGIN_LOAD);
        }
        return NULL;
    }
    plugin = init_device_plugin();
    if (plugin == NULL) {
        pm_on_error(path, strerror(errno), PM_ERROR_PLUGIN_LOAD);
        return NULL;
    }
    return plugin;
}

/* does not free the object being pointed to */
void pm_free_plugin(struct device_data *device_plugin) {
    device_plugin->device->free(device_plugin->device);
    free(device_plugin->name);
    free(device_plugin->path);
    dlclose(device_plugin->dlhandle);
}

static int pm_load_plugin(struct device_data *device_plugin) {
    device_plugin->dlhandle = dlopen(device_plugin->path, RTLD_NOW);
    if (device_plugin->dlhandle == NULL) {
        pm_on_error(device_plugin->path, dlerror(), PM_ERROR_PLUGIN_LOAD);
        return -1;
    }
    device_plugin->device = pm_init_plugin(device_plugin->dlhandle, device_plugin->path);
    if (device_plugin->device == NULL) {
        dlclose(device_plugin->dlhandle);
        return -1;
    }
    return 0;
}

static List *pm_load_plugins(List *plugin_names) {
    size_t num_plugin_names, i;
    List *device_plugins;
    device_plugins = list_new(sizeof(struct device_data), 4, 1.0, &util_list_allocator);
    num_plugin_names = list_num_elements(plugin_names);
    for (i = 0; i < num_plugin_names; ++i) {
        struct plugin_names *plugin_name;
        struct device_data device_plugin;
        plugin_name = list_get(plugin_names, i);
        device_plugin.name = plugin_name->name;
        device_plugin.path = plugin_name->path;
        if (pm_load_plugin(&device_plugin) < 0) {
            continue;
        }
        list_add(device_plugins, &device_plugin);
        /* set name and path to null to prevent double free's when freeing the plugin */
        plugin_name->name = NULL;
        plugin_name->path = NULL;
    }
    return device_plugins;
}

static struct plugin_names *pm_check_names(List *plugin_names, const char *name) {
    size_t num_plugin_names, i;
    num_plugin_names = list_num_elements(plugin_names);
    for (i = 0; i < num_plugin_names; ++i) {
        struct plugin_names *cur_plugin_name;
        cur_plugin_name = list_get(plugin_names, i);
        if (strcmp(name, cur_plugin_name->name) == 0) {
            return cur_plugin_name;
        }
    }
    return NULL;
}

static void pm_collect_device_plugin_name(List *plugin_names, const char *path) {
    struct plugin_names plugin_name;
    struct plugin_names *existing_plugin_name;
    size_t path_len;
    char *base_name;
    path_len = strlen(path);
    base_name = get_basename(path, path_len);
    plugin_name.path = safe_malloc(sizeof(char) * path_len + 1);
    strcpy(plugin_name.path, path);
    plugin_name.name = base_name;
    existing_plugin_name = pm_check_names(plugin_names, base_name);
    if (existing_plugin_name != NULL) {
        free(existing_plugin_name->name);
        free(existing_plugin_name->path);
        *existing_plugin_name = plugin_name;
    } else {
        list_add(plugin_names, &plugin_name);
    }
}

static void pm_collect_device_plugins_from_dir(const char *dir_path, const char *extension, List *plugin_names) {
    DIR *dir;
    struct dirent *dp;
    dir = opendir(dir_path);
    if (dir == NULL) {
        pm_on_error(dir_path, strerror(errno), PM_ERROR_OPENDIR);
        return;
    }
    errno = 0;
    while ((dp = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat file_stat;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }
        if (!build_path(path, dir_path, dp->d_name, sizeof(path))) {
            continue;
        }
        if (!check_extension(path, extension)) {
            continue;
        }
        if (stat(path, &file_stat) < 0) {
            pm_on_error(path, strerror(errno), PM_ERROR_PLUGIN_LOAD);
            continue;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            continue;
        }
        pm_collect_device_plugin_name(plugin_names, path);
    }
    if (errno != 0) {
        pm_on_error(dir_path, strerror(errno), PM_ERROR_OPENDIR);
    }
    closedir(dir);
}

void pm_set_on_error(void (*on_error)(const char *, const char *, enum pm_error, void *), void *data) {
    pm_on_error_ptr = on_error;
    pm_on_error_data = data;
}

static void pm_free_all_names(List *plugin_names) {
    size_t num_names, i;
    num_names = list_num_elements(plugin_names);
    for (i = 0; i < num_names; ++i) {
        struct plugin_names *cur_name;
        cur_name = list_get(plugin_names, i);
        free(cur_name->name);
        free(cur_name->path);
    }
    list_free(plugin_names);
}

List *pm_load_device_plugins(const char **dir_paths, size_t num_paths, const char *extension) {
    List *plugin_names, *loaded_plugins;
    size_t i;
    plugin_names = list_new(sizeof(struct plugin_names), 4, 1.0, &util_list_allocator);
    for (i = 0; i < num_paths; ++i) {
        pm_collect_device_plugins_from_dir(dir_paths[i], extension, plugin_names);
    }
    if (list_num_elements(plugin_names) == 0) {
        list_free(plugin_names);
        return NULL;
    }
    loaded_plugins = pm_load_plugins(plugin_names);
    if (list_num_elements(loaded_plugins) == 0) {
        list_free(loaded_plugins);
        loaded_plugins = NULL;
    }
    pm_free_all_names(plugin_names);
    return loaded_plugins;
}