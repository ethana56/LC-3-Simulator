#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hashmap.h"

struct bucket_node {
    void *key;
    struct bucket_node *next;
    unsigned long long hash;
};

struct hashmap {
    struct hashmap_allocator allocator;
    struct bucket_node **bucket_array;
    unsigned long long (*get_hash)(void *);
    int (*compare)(void *, void *);
    void (*free_key)(void *);
    size_t *sizes;
    size_t num_sizes;
    size_t cur_size;
    size_t num_elements;
    size_t element_size;
    size_t num_elements_grow;
    double load_factor;
    bool copy_elements;
};

struct hashmap_iterator {
    HashMap *hashmap;
    struct bucket_node *cur_node;
    size_t index;
    bool free_when_done;
};

static struct bucket_node **hashmap_allocate_bucket_array(struct hashmap_allocator *allocator, size_t num_buckets) {
    struct bucket_node **bucket_array;
    size_t i;
    bucket_array = allocator->alloc(sizeof(struct bucket_node *) * num_buckets);
    if (bucket_array == NULL) {
        return NULL;
    }
    for (i = 0; i < num_buckets; ++i) {
        bucket_array[i] = NULL;
    }
    return bucket_array;
}

HashMap *hashmap_new(struct hashmap_config *config) {
    HashMap *hashmap;
    hashmap = config->allocator.alloc(sizeof(HashMap));
    if (hashmap == NULL) {
        return NULL;
    }
    hashmap->allocator = config->allocator;
    hashmap->get_hash = config->get_hash;
    hashmap->compare = config->compare;
    hashmap->free_key = config->free_key;
    hashmap->sizes = hashmap->allocator.alloc(sizeof(size_t) * config->num_sizes);
    if (hashmap->sizes == NULL) {
        config->allocator.free(hashmap);
        return NULL;
    }
    memcpy(hashmap->sizes, config->sizes, sizeof(size_t) * config->num_sizes);
    hashmap->num_sizes = config->num_sizes;
    hashmap->load_factor = config->load_factor;
    hashmap->cur_size = 0;
    hashmap->num_elements_grow = hashmap->sizes[hashmap->cur_size] * hashmap->load_factor;
    hashmap->bucket_array = hashmap_allocate_bucket_array(&hashmap->allocator, hashmap->sizes[hashmap->cur_size]);
    if (hashmap->bucket_array == NULL) {
        config->allocator.free(hashmap->sizes);
        config->allocator.free(hashmap);
        return NULL;
    }
    hashmap->num_elements = 0;
    hashmap->copy_elements = config->copy_elements;
    hashmap->element_size = config->element_size;
    return hashmap;
}

static struct bucket_node *hashmap_bucket_node_create(struct hashmap_allocator *allocator, 
                                                    void *key, unsigned long long hash) {
    struct bucket_node *node;
    node = allocator->alloc(sizeof(struct bucket_node));
    if (node == NULL) {
        return NULL;
    }
    node->key = key;
    node->hash = hash;
    node->next = NULL;
    return node;
}

static int hashmap_add_key_to_bucket(HashMap *hashmap, void *key, struct bucket_node **bucket, unsigned long long hash) {
    struct bucket_node **cur_node;
    cur_node = bucket;
    while (*cur_node != NULL) {
        if (hashmap->compare((*cur_node)->key, key) == 0) {
            hashmap->free_key((*cur_node)->key);
            if (hashmap->copy_elements) {
                hashmap->allocator.free((*cur_node)->key);
            }
            (*cur_node)->key = key;
            return 0;
        }
        cur_node = &((*cur_node)->next);
    }
    *cur_node = hashmap_bucket_node_create(&hashmap->allocator, key, hash);
    if (*cur_node == NULL) {
        return -1;
    }
    ++hashmap->num_elements;
    return 0;
}

static void *hashmap_get_key_to_use(HashMap *hashmap, void *key) {
    void *key_to_use;
    if (hashmap->copy_elements) {
        key_to_use = hashmap->allocator.alloc(hashmap->element_size);
        if (key_to_use == NULL) {
            return NULL;
        }
        memcpy(key_to_use, key, hashmap->element_size);
    } else {
        key_to_use = key;
    }
    return key_to_use;
}

static struct bucket_node **hashmap_get_bucket(HashMap *hashmap, unsigned long long hash) {
    size_t index;
    index = hash % hashmap->sizes[hashmap->cur_size];
    return hashmap->bucket_array + index;
}

static int hashmap_add_key(HashMap *hashmap, void *key) {
    struct bucket_node **bucket;
    unsigned long long hash;
    void *key_to_use;
    key_to_use = hashmap_get_key_to_use(hashmap, key);
    if (key_to_use == NULL) {
        return -1;
    }
    hash = hashmap->get_hash(key_to_use);
    bucket = hashmap_get_bucket(hashmap, hash);
    if (hashmap_add_key_to_bucket(hashmap, key_to_use, bucket, hash) < 0) {
        if (key != key_to_use) {
            hashmap->allocator.free(key_to_use);
        }
        return -1;
    }
    return 0;
}

static void hashmap_add_node_to_bucket(HashMap *hashmap, struct bucket_node **bucket, struct bucket_node *node) {
    struct bucket_node *cur_node;
    if (*bucket == NULL) {
        *bucket = node;
        return;
    }
    cur_node = *bucket;
    while (cur_node->next != NULL) {
        cur_node = cur_node->next;
    }
    cur_node->next = node;
}

static void hashmap_rehash(HashMap *hashmap, struct bucket_node **bucket_array, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        struct bucket_node *cur_node;
        cur_node = bucket_array[i];
        while (cur_node != NULL) {
            struct bucket_node *next_node;
            size_t index;
            next_node = cur_node->next;
            cur_node->next = NULL;
            index = cur_node->hash % hashmap->sizes[hashmap->cur_size];
            hashmap_add_node_to_bucket(hashmap, (hashmap->bucket_array + index), cur_node);
            cur_node = next_node;
        }
    }
}

static int hashmap_resize(HashMap *hashmap) {
    struct bucket_node **new_bucket_array, **old_bucket_array;
    if (hashmap->cur_size == hashmap->num_sizes - 1) {
        return 0;
    }
    new_bucket_array = hashmap_allocate_bucket_array(&hashmap->allocator, 
                        hashmap->sizes[hashmap->cur_size + 1]);
    if (new_bucket_array == NULL) {
        return -1;
    }
    ++hashmap->cur_size;
    old_bucket_array = hashmap->bucket_array;
    hashmap->bucket_array = new_bucket_array;
    hashmap->num_elements_grow = hashmap->sizes[hashmap->cur_size] * hashmap->load_factor;
    hashmap_rehash(hashmap, old_bucket_array, hashmap->sizes[hashmap->cur_size - 1]);
    hashmap->allocator.free(old_bucket_array);
    return 0;
}

static struct bucket_node *hashmap_find_in_bucket(HashMap *hashmap, struct bucket_node **bucket, void *key) {
    struct bucket_node *cur_node;
    cur_node = *bucket;
    while (cur_node != NULL && hashmap->compare(cur_node->key, key) != 0) {
        cur_node = cur_node->next;
    }
    return cur_node;
}

static void hashmap_bucket_node_free(HashMap *hashmap, struct bucket_node *node) {
    hashmap->free_key(node->key);
    if (hashmap->copy_elements) {
        hashmap->allocator.free(node->key);
    }
    hashmap->allocator.free(node);
}

void *hashmap_get(HashMap *hashmap, void *key) {
    struct bucket_node **bucket, *found;
    bucket = hashmap_get_bucket(hashmap, hashmap->get_hash(key));
    found = hashmap_find_in_bucket(hashmap, bucket, key);
    return found == NULL ? NULL : found->key;
}

int hashmap_set(HashMap *hashmap, void *key) {
    if (hashmap->num_elements == hashmap->num_elements_grow) {
        if (hashmap_resize(hashmap) < 0) {
            return -1;
        }
    }
    return hashmap_add_key(hashmap, key);
}

static void hashmap_remove_key_from_bucket(HashMap *hashmap, struct bucket_node **bucket, void *key) {
    struct bucket_node *found;
    if (*bucket == NULL) {
        return;
    }
    if (hashmap->compare((*bucket)->key, key) == 0) {
        found = *bucket;
        *bucket = found->next;
    } else {
        struct bucket_node *cur_node;
        cur_node = *bucket;
        while (cur_node != NULL) {
            if (cur_node->next != NULL && hashmap->compare(cur_node->next->key, key) == 0) {
                break;
            }
            cur_node = cur_node->next;
        }
        found = cur_node->next;
        cur_node->next = cur_node->next->next;
    }
    hashmap_bucket_node_free(hashmap, found);
}

void hashmap_remove(HashMap *hashmap, void *key) {
    struct bucket_node **bucket;
    bucket = hashmap_get_bucket(hashmap, hashmap->get_hash(key));
    hashmap_remove_key_from_bucket(hashmap, bucket, key);
}

/* return value will be greater than or equal to hashmap size when there are no more buckets */
static size_t hashmap_find_next_bucket_index(HashMap *hashmap, size_t index) {
    size_t i, hashmap_size;
    hashmap_size = hashmap->sizes[hashmap->cur_size];
    for (i = index + 1; i < hashmap_size; ++i) {
        if (hashmap->bucket_array[i] != NULL) {
            break;
        }
    }
    return i;
}

static void hashmap_free_internal(HashMap *hashmap, bool call_free_key) {
    struct bucket_node *cur_node;
    size_t cur_bucket_index;
    for (cur_bucket_index = 0; cur_bucket_index < hashmap->sizes[hashmap->cur_size];) {
        cur_node = hashmap->bucket_array[cur_bucket_index];
        while (cur_node != NULL) {
            struct bucket_node *next_node;
            next_node = cur_node->next;
            if (call_free_key) {
                hashmap->free_key(cur_node->key);
                if (hashmap->copy_elements) {
                    hashmap->allocator.free(cur_node->key);
                }
            }
            hashmap->allocator.free(cur_node);
            cur_node = next_node;
        }
        cur_bucket_index = hashmap_find_next_bucket_index(hashmap, cur_bucket_index);
    }
    hashmap->allocator.free(hashmap->bucket_array);
    hashmap->allocator.free(hashmap->sizes);
    hashmap->allocator.free(hashmap);
}

void hashmap_free(HashMap *hashmap) {
    hashmap_free_internal(hashmap, true);
}

static void hashmap_iterator_find_next_bucket(HashMapIterator *iterator) {
    iterator->index = hashmap_find_next_bucket_index(iterator->hashmap, iterator->index);
    if (iterator->index >= iterator->hashmap->sizes[iterator->hashmap->cur_size]) {
        return;
    }
    
    iterator->cur_node = iterator->hashmap->bucket_array[iterator->index];
}

static void hashmap_iterator_initiate(HashMapIterator *iterator, HashMap *hashmap, bool free_when_done) {
    iterator->hashmap = hashmap;
    iterator->index = 0;
    if (iterator->hashmap->bucket_array[iterator->index] == NULL) {
        hashmap_iterator_find_next_bucket(iterator);
    } else {
        iterator->cur_node = iterator->hashmap->bucket_array[iterator->index];
    }
    iterator->free_when_done = free_when_done;
}

static void hashmap_iterator_find_next_node(HashMapIterator *iterator) {
    if (iterator->cur_node->next == NULL) {
        hashmap_iterator_find_next_bucket(iterator);
    } else {
        iterator->cur_node = iterator->cur_node->next;
    }
}

void *hashmap_iterator_next(HashMapIterator *iterator) {
    void *key;
    size_t cur_hashmap_size;
    cur_hashmap_size = iterator->hashmap->sizes[iterator->hashmap->cur_size];
    if (iterator->index >= cur_hashmap_size) {
        return NULL;
    }
    key = iterator->cur_node->key;
    hashmap_iterator_find_next_node(iterator);
    return key;
}

HashMapIterator *hashmap_to_iterator(HashMap *hashmap) {
    HashMapIterator *iterator;
    iterator = hashmap->allocator.alloc(sizeof(HashMapIterator));
    if (iterator == NULL) {
        return NULL;
    }
    hashmap_iterator_initiate(iterator, hashmap, true);
    return iterator;
}

HashMapIterator *hashmap_get_iterator(HashMap *hashmap) {
    HashMapIterator *iterator;
    iterator = hashmap->allocator.alloc(sizeof(HashMapIterator));
    if (iterator == NULL) {
        return NULL;
    }
    hashmap_iterator_initiate(iterator, hashmap, false);
    return iterator;
}

void hashmap_iterator_free(HashMapIterator *iterator) {
    void (*allocator_free)(void *);
    allocator_free = iterator->hashmap->allocator.free;
    if (iterator->free_when_done) {
        hashmap_free(iterator->hashmap);
    }
    allocator_free(iterator);
}

static void hashmap_add_element_to_array(void *array, 
                                        void *element, 
                                        size_t index, 
                                        size_t element_size)
{
    memcpy((char *)array + (index * element_size), element, element_size);
}

void *hashmap_to_list(HashMap *hashmap, size_t *num_elements) {
    void *array;
    size_t i, cur_element, hashmap_size;
    array = hashmap->allocator.alloc(hashmap->element_size * hashmap->num_elements);
    if (array == NULL) {
        return NULL;
    }
    cur_element = 0;
    hashmap_size = hashmap->sizes[hashmap->cur_size];
    for (i = 0; i < hashmap_size; ++i) {
        struct bucket_node *node;
        node = hashmap->bucket_array[i];
        while (node != NULL) {
            hashmap_add_element_to_array(array, node->key, cur_element, hashmap->element_size);
            ++cur_element;
            node = node->next;
        }
    }
    *num_elements = hashmap->num_elements;
    hashmap_free_internal(hashmap, false);
    return array;
}
