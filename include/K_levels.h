#pragma once

#include "K_file.h"   // IWYU pragma: export
#include "K_memory.h" // IWYU pragma: export

typedef struct {
    const char* name;
    Uint32 hash;
} Level;

void levels_init(), levels_teardown();

const Level *get_level(const char*), *get_level_key(TinyHash);
yyjson_doc* load_level_json(const char*, const char**);
