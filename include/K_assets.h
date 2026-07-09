#pragma once

#include "K_memory.h" // IWYU pragma: export

typedef Uint8 AssetKeepLevel;
enum {
    AKL_NEVER,
    AKL_ONCE,
    AKL_ALWAYS,
};

typedef struct {
    const char* name;
    AssetKeepLevel keep;
} AssetBase;

#define ASSET_HEAD(M, T, A)                                                                                            \
    void load_##A(const char*, AssetKeepLevel);                                                                        \
    void load_##A##_num(const char*, Uint32, AssetKeepLevel);                                                          \
    const T* get_##A(const char*);                                                                                     \
    const T* get_##A##_key(TinyHash);                                                                                  \
    const T* fetch_##A(const char*, AssetKeepLevel);                                                                   \
    void clear_##M();

#define ASSET_SRC(M, T, A)                                                                                             \
    void load_##A##_num(const char* pattern, Uint32 n, AssetKeepLevel keep) {                                          \
        for (Uint32 i = 0; i < n; i++)                                                                                 \
            load_##A(fmt(pattern, i), keep);                                                                           \
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
    const T* fetch_##A(const char* name, AssetKeepLevel keep) {                                                        \
        load_##A(name, keep);                                                                                          \
        return get_##A(name);                                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    void clear_##M() {                                                                                                 \
        clear_asset_map(&(M), nuke_##A);                                                                               \
    }

#define CHECK_ASSET(M)                                                                                                 \
    if (name == NULL)                                                                                                  \
        return;                                                                                                        \
                                                                                                                       \
    AssetBase* __base = (AssetBase*)TinyDictGet(&(M), name);                                                           \
    if (__base != NULL) {                                                                                              \
        if (__base->keep < keep)                                                                                       \
            __base->keep = keep;                                                                                       \
                                                                                                                       \
        return;                                                                                                        \
    }

void clear_asset_map(TinyMap*, void (*)(void*));
void clear_assets();
