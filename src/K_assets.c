#include "K_assets.h"
#include "K_audio.h"
#include "K_video.h"

void clear_assets() {
	clear_textures(), clear_fonts(), clear_sounds(), clear_tracks();
}

void clear_asset_map_PRO(StTinyMap** target, void (*nuke)(void*)) {
	StTinyMap* new = NewTinyMap();

	StTinyMapIter it = StMapIter(*target);
	while (StMapNext(&it)) {
		StTinyBucket* bucket = it.at;

		AssetBase* asset = bucket->data;
		if (!asset->transient)
			continue;
		StMapPut(new, bucket->key, asset, (int)bucket->size)->cleanup = nuke;
		bucket->cleanup = NULL;
	}

	FreeTinyMap(*target);
	*target = new;
}
