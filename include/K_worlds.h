#pragma once

#include "K_memory.h" // IWYU pragma: keep

typedef struct {
    const char* name;
    Uint32 hash;
} World;

void worlds_init(), worlds_teardown();

const World *get_world(const char*), *get_world_key(TinyHash);
const char *next_world_from(const char*), *last_world_from(const char*);
