#include "K_editor.h"

static Bool editing = FALSE;

Bool is_editing_level() {
	return editing;
}

void set_editing_level(Bool value) {
	editing = value;
}

void editor_baton_pass(GamePlayer* player, GameActor* self) {}
