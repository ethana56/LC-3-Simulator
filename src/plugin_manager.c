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
#include <stdbool.h>

#include "list.h"
#include "plugin_manager.h"
#include "simulator.h"
#include "device.h"
#include "util.h"
#include "hashmap.h"
#include "pm_name_manager.h"

struct plugin_manager {
    HashMap *hashmap;
    void (*on_error)(const char *, const char *, enum pm_error, void *);
    void *on_error_data;
};

struct plugin_manager_entry {
    void *dlhandle;
    char *name;
    char *path;
    struct device *device;
};

struct plugin_manager_iterator {
    HashMapIterator *map_iterator;
};

static const char *func_null_error_string = "init_device_plugin is null";

static void pm_on_error(PluginManager *plugin_manager, const char *path, const char *error_string, enum pm_error error_type) {
    if (plugin_manager->on_error != NULL) {
        plugin_manager->on_error(path, error_string, error_type, plugin_manager->on_error_data);
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

static struct device *pm_init_plugin(PluginManager *plugin_manager, void *dlhandle, const char *path) {
    struct device *(*init_device_plugin)(void);
    struct device *plugin;
    char *dl_error_string;
    init_device_plugin = dlsym(dlhandle, "init_device_plugin");
    if (init_device_plugin == NULL) {
        const char *final_error_string;
        dl_error_string = dlerror();
        final_error_string = (dl_error_string == NULL ? func_null_error_string : dl_error_string);
        pm_on_error(plugin_manager, path, final_error_string, PM_ERROR_PLUGIN_LOAD);
        return NULL;
    }
    plugin = init_device_plugin();
    if (plugin == NULL) {
        pm_on_error(plugin_manager, path, strerror(errno), PM_ERROR_PLUGIN_LOAD);
        return NULL;
    }
    return plugin;
}

static int pm_load_plugin(PluginManager *plugin_manager, struct plugin_manager_entry *entry) {
    entry->dlhandle = dlopen(entry->path, RTLD_NOW);
    if (entry->dlhandle == NULL) {
        pm_on_error(plugin_manager, entry->path, dlerror(), PM_ERROR_PLUGIN_LOAD);
        return -1;
    }
    entry->device = pm_init_plugin(plugin_manager, entry->dlhandle, entry->path);
    if (entry->device == NULL) {
        dlclose(entry->dlhandle);
        return -1;
    }
    return 0;
}

static void pm_load_plugins(PluginManager *plugin_manager, 
                                PMNameManagerIterator *plugin_names_iterator) 
{
    struct plugin_names *names;
    while ((names = pmnm_iterator_next(plugin_names_iterator)) != NULL) {
        printf("names: name: %s, path: %s\n", names->name, names->path);
        struct plugin_manager_entry entry;
        entry.name = names->name;
        entry.path = names->path;
        if (pm_load_plugin(plugin_manager, &entry) < 0) {
            free(entry.name);
            free(entry.path);
            continue;
        }
        hashmap_set(plugin_manager->hashmap, &entry);
    }
}

static void pm_collect_device_plugins_from_dir(PluginManager *plugin_manager, const char *dir_path, const char *extension, PMNameManager *plugin_names) {
    DIR *dir;
    struct dirent *dp;
    dir = opendir(dir_path);
    if (dir == NULL) {
        pm_on_error(plugin_manager, dir_path, strerror(errno), PM_ERROR_OPENDIR);
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
            pm_on_error(plugin_manager, path, strerror(errno), PM_ERROR_PLUGIN_LOAD);
            continue;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            continue;
        }
        pmnm_add_path(plugin_names, path);
    }
    if (errno != 0) {
        pm_on_error(plugin_manager, dir_path, strerror(errno), PM_ERROR_OPENDIR);
    }
    closedir(dir);
}

static PMNameManager *pm_collect_device_plugin_names(PluginManager *plugin_manager, List *dir_paths, const char *extension) {
    PMNameManager *plugin_names;
    size_t num_paths, i;
    plugin_names = pmnm_new();
    num_paths = list_num_elements(dir_paths);
    for (i = 0; i < num_paths; ++i) {
        pm_collect_device_plugins_from_dir(plugin_manager, *(char **)list_get(dir_paths, i), 
                                            extension, plugin_names);
    }
    return plugin_names;
}

static unsigned long long pm_hash(void *key) {
    struct plugin_manager_entry *entry;
    entry = key;
    return string_hash(entry->name);
}

static int pm_compare(void *one, void *two) {
    struct plugin_manager_entry *entry_one, *entry_two;
    entry_one = one;
    entry_two = two;
    return strcmp(entry_one->name, entry_two->name);
}

static void pm_free_key(void *key) {
    struct plugin_manager_entry *entry;
    entry = key;
    free(entry->name);
    free(entry->path);
    entry->device->free(entry->device);
    dlclose(entry->dlhandle);
}

static HashMap *pm_build_plugins_hashmap(void) {
    size_t sizes[] = {11, 23};
    struct hashmap_config config = {
        .allocator = {
            .alloc = safe_malloc,
            .free = free
        },
        .get_hash = pm_hash,
        .compare = pm_compare,
        .free_key = pm_free_key,
        .element_size = sizeof(struct plugin_manager_entry),
        .load_factor = .75,
        .sizes = sizes,
        .num_sizes = 2,
        .copy_elements = true,
    };
    return hashmap_new(&config);
}

PluginManager *pm_new(void (*on_error)(const char *, const char *, enum pm_error, void *), void *data) {
    PluginManager *plugin_manager;
    plugin_manager = safe_malloc(sizeof(PluginManager));
    plugin_manager->hashmap = pm_build_plugins_hashmap();
    plugin_manager->on_error = on_error;
    plugin_manager->on_error_data = data;
    return plugin_manager;
}

void pm_free(PluginManager *plugin_manager) {
    hashmap_free(plugin_manager->hashmap);
    free(plugin_manager);
}

void pm_load_device_plugins(PluginManager *plugin_manager, List *dir_paths, const char *extension) {
    PMNameManager *plugin_names;
    PMNameManagerIterator *plugin_names_iterator;
    plugin_names = pm_collect_device_plugin_names(plugin_manager, dir_paths, extension);
    plugin_names_iterator = pmnm_to_iterator(plugin_names);
    pm_load_plugins(plugin_manager, plugin_names_iterator);
    pmnm_iterator_free(plugin_names_iterator);
}

PluginManagerIterator *pm_get_iterator(PluginManager *plugin_manager) {
    PluginManagerIterator *iterator;
    iterator = safe_malloc(sizeof(PluginManagerIterator));
    iterator->map_iterator = hashmap_get_iterator(plugin_manager->hashmap);
    return iterator;
}

void pm_remove_plugin(PluginManager *plugin_manager, char *name) {
    hashmap_remove(plugin_manager->hashmap, name);
}

struct pm_device_data *pm_iterator_next(PluginManagerIterator *iterator, struct pm_device_data *device_data) {
    struct plugin_manager_entry *entry;
    entry = hashmap_iterator_next(iterator->map_iterator);
    if (entry == NULL) {
        return NULL;
    }
    device_data->name = entry->name;
    device_data->path = entry->path;
    device_data->device = entry->device;
    return device_data;
}

void pm_iterator_free(PluginManagerIterator *iterator) {
    hashmap_iterator_free(iterator->map_iterator);
    free(iterator);
}