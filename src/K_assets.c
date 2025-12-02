#include "K_assets.h"
#include "K_audio.h"
#include "K_video.h"

void clear_assets() {
	clear_textures(), clear_fonts(), clear_sounds(), clear_tracks();
}
