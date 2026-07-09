#pragma once

#include "K_memory.h" // IWYU pragma: keep

typedef struct {
    const char* name;
    Uint32 hash;
} Level;

void levels_init(), levels_teardown();

const Level *get_level(const char*), *get_level_key(TinyHash);
