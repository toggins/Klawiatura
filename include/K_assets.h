#pragma once

#include "K_memory.h" // IWYU pragma: export
#include "K_misc.h"

typedef struct {
    const char* name;
    Bool persistent;
} AssetBase;

#define ASSET_HEAD(M, T, A)                                                                                            \
    void load_##A(const char*, Bool);                                                                                  \
    void load_##A##_num(const char*, Uint32, Bool);                                                                    \
    const T* get_##A(const char*);                                                                                     \
    const T* get_##A##_key(TinyHash);                                                                                  \
    const T* fetch_##A(const char*, Bool);                                                                             \
    void clear_##M();

#define ASSET_SRC(M, T, A)                                                                                             \
    void load_##A##_num(const char* pattern, Uint32 n, Bool persistent) {                                              \
        load_asset_num(pattern, n, persistent, load_##A);                                                              \
    }                                                                                                                  \
                                                                                                                       \
    const T* get_##A(const char* name) {                                                                               \
        return get_##A##_key(StHashStr(name));                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    const T* get_##A##_key(TinyHash key) {                                                                             \
        return (T*)TinyMapGet(&(M), key);                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    const T* fetch_##A(const char* name, Bool persistent) {                                                            \
        load_##A(name, persistent);                                                                                    \
        return get_##A(name);                                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    void clear_##M() {                                                                                                 \
        clear_asset_map(&(M), nuke_##A);                                                                               \
    }

void clear_asset_map(TinyMap*, void (*)(void*));
void clear_assets();
void load_asset_num(const char*, Uint32, Bool, void (*)(const char*, Bool));
