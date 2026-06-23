#pragma once

#include "K_memory.h" // IWYU pragma: export

#define LFMT(key, ...) handle_lfmt(key, ##__VA_ARGS__, '\0')

typedef struct {
    const char* name;
    TinyMap strings;
} Language;

void locale_init(), locale_teardown();

void apply_language(const char*);
const char* handle_lfmt(const char*, ...);

void language_iterate_start();
const Language* language_iterate_next();
