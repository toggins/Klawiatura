#pragma once

#include "K_memory.h" // IWYU pragma: keep

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
		StTinyMap* M##_new = NewTinyMap();                                                                     \
                                                                                                                       \
		StTinyMapIter it = StMapIter(M);                                                                       \
		while (StMapNext(&it)) {                                                                               \
			StTinyBucket* bucket = it.at;                                                                  \
                                                                                                                       \
			T* asset = bucket->data;                                                                       \
			if (!asset->transient)                                                                         \
				continue;                                                                              \
			StMapPut(M##_new, bucket->key, asset, bucket->size)->cleanup = nuke_##A;                       \
			bucket->cleanup = NULL;                                                                        \
		}                                                                                                      \
                                                                                                                       \
		FreeTinyMap(M);                                                                                        \
		M = M##_new;                                                                                           \
	}

void clear_assets();
