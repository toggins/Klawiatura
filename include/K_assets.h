#pragma once

#define CLEAR_ASSETS_IMPL(m, t, n)                                                                                     \
	void clear_##m() {                                                                                             \
		StTinyMap* m##_new = NewTinyMap();                                                                     \
                                                                                                                       \
		StTinyMapIter it = StMapIter(m);                                                                       \
		while (StMapNext(&it)) {                                                                               \
			StTinyBucket* bucket = it.at;                                                                  \
                                                                                                                       \
			t* asset = bucket->data;                                                                       \
			if (!asset->transient)                                                                         \
				continue;                                                                              \
			StMapPut(m##_new, bucket->key, asset, bucket->size)->cleanup = nuke_##n;                       \
			bucket->cleanup = NULL;                                                                        \
		}                                                                                                      \
                                                                                                                       \
		FreeTinyMap(m);                                                                                        \
		m = m##_new;                                                                                           \
	}

#include "K_audio.h" // IWYU pragma: keep
#include "K_video.h" // IWYU pragma: keep

void clear_assets();
