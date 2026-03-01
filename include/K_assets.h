#pragma once

#include "K_memory.h" // IWYU pragma: keep
#include "K_misc.h"

typedef struct {
	char* name;
	Bool transient;
} AssetBase;

#define ASSET_HEAD(M, T, A)                                                                                            \
	void load_##A##_pro(const char*, Bool);                                                                        \
	void load_##A(const char*), load_transient_##A(const char*);                                                   \
	void load_##A##_num(const char*, Uint32);                                                                      \
	const T* get_##A(const char*);                                                                                 \
	const T* get_##A##_key(StTinyKey);                                                                             \
	void clear_##M();

#define ASSET_SRC(M, T, A)                                                                                             \
	void load_##A(const char* name) {                                                                              \
		load_##A##_pro(name, FALSE);                                                                           \
	}                                                                                                              \
                                                                                                                       \
	void load_transient_##A(const char* name) {                                                                    \
		load_##A##_pro(name, TRUE);                                                                            \
	}                                                                                                              \
                                                                                                                       \
	void load_##A##_num(const char* pattern, Uint32 n) {                                                           \
		load_num_pro(pattern, n, load_##A);                                                                    \
	}                                                                                                              \
                                                                                                                       \
	const T* get_##A(const char* name) {                                                                           \
		return get_##A##_key(StHashStr(name));                                                                 \
	}                                                                                                              \
                                                                                                                       \
	const T* get_##A##_key(StTinyKey key) {                                                                        \
		return StMapGet(M, key);                                                                               \
	}                                                                                                              \
                                                                                                                       \
	void clear_##M() {                                                                                             \
		clear_asset_map_PRO(&(M), nuke_##A);                                                                   \
	}

void load_num_pro(const char*, Uint32, void (*)(const char*));
void clear_asset_map_PRO(StTinyMap** target, void (*nuke)(void*));
void clear_assets();
