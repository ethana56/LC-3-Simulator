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
#include "device_plugin.h"
#include "plugin_manager.h"

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
    if (combined_size >= dst_size) {
        return 0;
    }
    return 1;
}

static int init_plugin(struct device_plugin *plugin, const char *path, char *error_string) {
    int (*init_device_plugin)(struct device_plugin *);
    char *dl_error_string;
    init_device_plugin = dlsym(plugin->data, "init_device_plugin");
    if (init_device_plugin == NULL) {
        dl_error_string = dlerror();
	    if (dl_error_string == NULL) {
            create_func_null_error_string(error_string, path);       		
	    } else {
	        cpy_error_string(error_string, dl_error_string);
	    }
	    return -1;
    }
    if ((*init_device_plugin)(plugin) < 0) {
	    cpy_error_string(error_string, strerror(errno));
        return -1;
    }
    return 0;
}

static int add_device_plugin(List *device_plugins, const char *path, char *error_string) {
    struct device_plugin *plugin;
    plugin = malloc(sizeof(struct device_plugin));
    if (plugin == NULL) {
	cpy_error_string(error_string, strerror(errno));    
        return -1;
    }
    plugin->data = dlopen(path, RTLD_NOW);
    if (plugin->data == NULL) { 
        cpy_error_string(error_string, dlerror());    
        free(plugin);
        return -1;
    }
    if (init_plugin(plugin, path, error_string) < 0) {    
        return -1;
    }
    if (list_add(device_plugins, plugin) < 0) {
        cpy_error_string(error_string, strerror(errno));    
        plugin->cleanup();
        free(plugin);
        return -1;
    }
    return 0;
}

void pm_free_plugin(struct device_plugin *plugin) {
    plugin->cleanup();
    dlclose(plugin->data); 
    free(plugin);
}

static void device_plugins_list_free(List *device_plugins) {
    size_t num_plugins, i;
    num_plugins = list_size(device_plugins);
    for (i = 0; i < num_plugins; ++i) {
        struct device_plugin *plugin;
        plugin = list_get(device_plugins, i);
        pm_free_plugin(plugin);
    }
    list_free(device_plugins);
}

struct device_plugin **pm_load_device_plugins(const char *dir_path, size_t *num_plugins, char *error_string) {
    List *device_plugins = NULL;
    DIR *dir;
    struct dirent *dp;
    int readdir_error = 0;
    dir = opendir(dir_path);
    if (dir == NULL) {
        goto err_str;
    }
    device_plugins = new_List(2, 1.0);
    if (device_plugins == NULL) {
        goto err_str;
    }
    errno = 0;
    while ((dp = readdir(dir)) != NULL) {
        struct device_plugin *plugin;
        char path[PATH_MAX + 1];
        struct stat file_stat;
	    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
	    }
	    if (!build_path(path, dir_path, dp->d_name, sizeof(path))) {
            continue;
	    }
	    if (stat(path, &file_stat) < 0) {
            goto err_str;
	    }
	    if (S_ISDIR(file_stat.st_mode)) {
            continue;
	    }
        if (add_device_plugin(device_plugins, path, error_string) < 0) {	
            goto err;
	    }
    }
    if (errno != 0) {
        goto err_str;
    }
    closedir(dir);
    return (struct device_plugin **)list_free_and_return_as_array(device_plugins, num_plugins);

err_str:
    cpy_error_string(error_string, strerror(errno));
err:
    if (device_plugins != NULL) device_plugins_list_free(device_plugins);
    if (dir != NULL) closedir(dir);    
    return NULL;
}
