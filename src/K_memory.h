#pragma once

#include <SDL3/SDL_stdinc.h>

// Hash maps
#define HASH_TOMBSTONE ((char*)(-1))

struct KeyValuePair {
    char* key;
    void* value;
};

struct HashMap {
    struct KeyValuePair* items;
    size_t count, length, capacity;
};

struct HashMap* create_hash_map();
void destroy_hash_map(struct HashMap*, void (*)(void*));
bool to_hash_map(struct HashMap*, const char*, void*);
void* from_hash_map(struct HashMap*, const char*);
