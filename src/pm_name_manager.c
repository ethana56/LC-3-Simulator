#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pm_name_manager.h"
#include "hashmap.h"
#include "util.h"
#include "list.h"

struct pm_name_manager {
    HashMap *hashmap;
};

struct pm_name_manager_iterator {
    HashMapIterator *map_iterator;
};

static size_t hashmap_sizes[] = {11, 23};
static const size_t NUM_SIZES = 2;

static unsigned long long hash(void *key) {
    struct plugin_names *names;
    names = key;
    return string_hash(names->name);
}

static int hash_compare(void *key_one, void *key_two) {
    struct plugin_names *name_one, *name_two;
    name_one = key_one;
    name_two = key_two;
    return strcmp(name_one->name, name_two->name);
}

static void free_key(void *key) {
    /* Intentionally left blank */
}

static HashMap *pmnm_build_hashmap(void) {
    HashMap *hashmap;
    struct hashmap_config config = {
        .allocator = {
            .alloc = safe_malloc,
            .free = free
        },
        .compare = hash_compare,
        .get_hash = hash,
        .free_key = free_key,
        .element_size = sizeof(struct plugin_names),
        .sizes = hashmap_sizes,
        .num_sizes = NUM_SIZES,
        .load_factor = 0.75,
        .copy_elements = true,
    };
    hashmap = hashmap_new(&config);
    return hashmap;
}

PMNameManager *pmnm_new(void) {
    PMNameManager *name_manager;
    name_manager = safe_malloc(sizeof(PMNameManager));
    name_manager->hashmap = pmnm_build_hashmap();
    return name_manager;
}

void pmnm_add_path(PMNameManager *name_manager, const char *path) {
    struct plugin_names names;
    size_t path_len;
    path_len = strlen(path);
    names.path = safe_malloc((sizeof(char) * path_len) + 1);
    strcpy(names.path, path);
    names.name = get_basename(path, path_len);
    hashmap_set(name_manager->hashmap, &names);
}

PMNameManagerIterator *pmnm_to_iterator(PMNameManager *name_manager) {
    PMNameManagerIterator *iterator;
    iterator = safe_malloc(sizeof(PMNameManagerIterator));
    iterator->map_iterator = hashmap_to_iterator(name_manager->hashmap);
    free(name_manager);
    return iterator;
}

void pmnm_iterator_free(PMNameManagerIterator *iterator) {
    hashmap_iterator_free(iterator->map_iterator);
    free(iterator);
}

struct plugin_names *pmnm_iterator_next(PMNameManagerIterator *iterator) {
    return hashmap_iterator_next(iterator->map_iterator);
}