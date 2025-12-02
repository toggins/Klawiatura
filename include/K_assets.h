#pragma once

#include "K_memory.h" // IWYU pragma: keep

typedef struct {
	const char* name;
	bool transient;
} AssetBase;

#define ASSET_HEAD(M, T, A)                                                                                            \
	void load_##A(const char*, bool);                                                                              \
	const T* get_##A(const char*);                                                                                 \
	const T* get_##A##_key(StTinyKey);                                                                             \
	void clear_##M();

#define ASSET_SRC(M, T, A)                                                                                             \
	const T* get_##A(const char* name) {                                                                           \
		return get_##A##_key(StHashStr(name));                                                                 \
	}                                                                                                              \
                                                                                                                       \
	const T* get_##A##_key(StTinyKey key) {                                                                        \
		return StMapGet(M, key);                                                                               \
	}                                                                                                              \
                                                                                                                       \
	void clear_##M() {                                                                                             \
		clear_asset_map_PRO(&M, nuke_##A);                                                                     \
	}

void clear_asset_map_PRO(StTinyMap** target, void (*nuke)(void*));
void clear_assets();
