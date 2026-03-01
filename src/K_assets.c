#include "K_assets.h"
#include "K_audio.h"
#include "K_video.h"

void load_num_pro(const char* pattern, Uint32 n, void (*load)(const char*)) {
	static char buf[256] = {0};
	for (Uint32 i = 0; i < n; i++)
		SDL_snprintf(buf, sizeof(buf), pattern, i), load(buf);
}

void clear_assets() {
	clear_textures(), clear_fonts(), clear_sounds(), clear_tracks();
}

void clear_asset_map_PRO(StTinyMap** target, void (*nuke)(void*)) {
	StTinyMap* new = NewTinyMap();

	StIterator it = StMapIter(*target);
	while (StIterNext(&it)) {
		StTinyBucket* bucket = it.bucket;

		AssetBase* asset = bucket->data;
		if (!asset->transient)
			continue;
		StMapPut(new, bucket->key, asset, (int)bucket->size)->cleanup = nuke;
		bucket->cleanup = NULL;
	}

	FreeTinyMap(*target);
	*target = new;
}
