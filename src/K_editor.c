#include "K_editor.h"
#include "K_video.h"

static Bool editing = FALSE;

Bool is_editing_level() {
	return editing && !is_in_netgame();
}

void set_editing_level(Bool value) {
	editing = value;
}

void load_editor() {
	load_texture("tiles/tank");
}

void editor_baton_pass(GamePlayer* player, GameActor* self) {}

void draw_editor() {
	batch_reset();
	batch_pos(B_XY(0, 0));
	batch_sprite("tiles/tank");
}
