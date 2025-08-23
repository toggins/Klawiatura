#include "K_memory.h"
#include "K_log.h"

// Abstract hash maps
// You can either free values manually or nuke them alongside the maps.
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
#define HASH_CAPACITY 16
#define FNV_OFFSET 0x811c9dc5
#define FNV_PRIME 0x01000193

static uint32_t hash_key(const char* key) {
    uint32_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint32_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

struct HashMap* create_hash_map() {
    struct HashMap* map = SDL_malloc(sizeof(struct HashMap));
    if (map == NULL)
        FATAL("HashMap fail");
    SDL_memset(map, 0, sizeof(struct HashMap));

    map->capacity = HASH_CAPACITY;
    map->items = SDL_malloc(HASH_CAPACITY * sizeof(struct KeyValuePair));
    if (map->items == NULL)
        FATAL("HashMap items fail");
    SDL_memset(map->items, 0, HASH_CAPACITY * sizeof(struct KeyValuePair));

    return map;
}

void destroy_hash_map(struct HashMap* map, void (*nuker)(void*)) {
    for (size_t i = 0; i < map->capacity; i++) {
        struct KeyValuePair* kvp = &map->items[i];
        if (kvp->key != NULL && kvp->key != HASH_TOMBSTONE)
            SDL_free(kvp->key);
        if (nuker != NULL && kvp->value != NULL)
            nuker(kvp->value);
    }

    SDL_free(map->items);
    SDL_free(map);
}

static bool to_hash_map_direct(
    struct KeyValuePair* items, size_t* count, size_t* length, size_t capacity, const char* key, void* value
) {
    size_t index = (size_t)hash_key(key) % capacity;
    struct KeyValuePair* kvp = &(items[index]);
    while (kvp->key != NULL) {
        if (kvp->key != HASH_TOMBSTONE && SDL_strcmp(key, kvp->key) == 0) {
            if (kvp->value != NULL)
                return false;
            kvp->value = value;
            return true;
        }

        index = (index + 1) % capacity;
        kvp = &(items[index]);
    }

    items[index].key = count == NULL ? (char*)key : SDL_strdup(key);
    items[index].value = value;
    if (count != NULL) {
        (*count)++;
        (*length)++;
    }
    return true;
}

static void expand_hash_map(struct HashMap* map) {
    const size_t new_capacity = map->capacity * 2;
    if (new_capacity < map->capacity)
        FATAL("Capacity overflow in HashMap");

    const size_t old_capacity = map->capacity;
    const size_t new_size = new_capacity * sizeof(struct KeyValuePair);
    struct KeyValuePair* items = SDL_malloc(new_size);
    if (items == NULL)
        FATAL("expand_hash_map items fail");
    SDL_memset(items, 0, new_size);

    for (size_t i = 0; i < map->capacity; i++) {
        struct KeyValuePair* kvp = &(map->items[i]);
        if (kvp->key != NULL) {
            if (kvp->key == HASH_TOMBSTONE)
                map->length--;
            else
                to_hash_map_direct(items, NULL, NULL, new_capacity, kvp->key, kvp->value);
        }
    }

    SDL_free(map->items);
    map->items = items;
    map->capacity = new_capacity;
}

bool to_hash_map(struct HashMap* map, const char* key, void* value) {
    if (map->length >= map->capacity / 2)
        expand_hash_map(map);
    return to_hash_map_direct(map->items, &(map->count), &(map->length), map->capacity, key, value);
}

void* from_hash_map(struct HashMap* map, const char* key) {
    size_t index = (size_t)hash_key(key) % map->capacity;
    const struct KeyValuePair* kvp = &map->items[index];
    while (kvp->key != NULL) {
        if (kvp->key != HASH_TOMBSTONE && SDL_strcmp(key, kvp->key) == 0)
            return kvp->value;

        index = (index + 1) % map->capacity;
        kvp = &map->items[index];
    }

    return NULL;
}
