#include "K_assets.h"
#include "K_audio.h"
#include "K_video.h"

void clear_assets() {
    clear_textures();
    clear_sprites();
    clear_fonts();
    clear_sounds();
    clear_tracks();
}

void clear_asset_map(TinyMap* target, void (*nuke)(void*)) {
    TinyMap new = {0};

    TinyMapIterator it = TinyMapIter(target);
    while (TinyMapNext(&it)) {
        TinyBucket* bucket = it.bucket;

        AssetBase* asset = bucket->data;
        switch (asset->keep) {
        default:
            continue;
        case AKL_ONCE:
            asset->keep = AKL_NEVER;
        case AKL_ALWAYS:
            break;
        }

        TinyMapPut(&new, bucket->hash, asset, (int)bucket->data_size)->cleanup = nuke;
        bucket->cleanup = NULL;
    }

    FreeTinyMap(target);
    *target = new;
}
