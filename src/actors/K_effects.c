#include "K_game.h"

// =========
// EXPLOSION
// =========

enum {
	VAL_EXPLODE_FRAME = VAL_CUSTOM,
};

static void load_explode() {
	load_texture("effects/explode");
	load_texture("effects/explode2");
	load_texture("effects/explode3");
}

static void tick_explode(GameActor* actor) {
	VAL(actor, VAL_EXPLODE_FRAME) += 24L;
	if (VAL(actor, VAL_EXPLODE_FRAME) >= 300L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_explode(const GameActor* actor) {
	const char* tex;
	switch (VAL(actor, VAL_EXPLODE_FRAME) / 100L) {
	default:
		tex = "effects/explode";
		break;
	case 1:
		tex = "effects/explode2";
		break;
	case 2:
		tex = "effects/explode3";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

const GameActorTable TAB_EXPLODE = {.load = load_explode, .tick = tick_explode, .draw = draw_explode};
