#include "K_game.h"

enum {
	VAL_PROP_FRAME,
};

static void create(GameActor* actor) {
	VAL(actor, PROP_FRAME) = rng(100L);
}

// =====
// CLOUD
// =====

static void load_cloud() {
	load_texture_num("props/cloud%u", 3L);
}

static void draw_cloud(const GameActor* actor) {
	const char* tex = NULL;
	switch (((int)((float)game_state.time / 12.5f) + VAL(actor, PROP_FRAME)) % 4L) {
	default:
		tex = "props/cloud0";
		break;
	case 1L:
	case 3L:
		tex = "props/cloud1";
		break;
	case 2L:
		tex = "props/cloud2";
		break;
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_CLOUD = {.load = load_cloud, .create = create, .draw = draw_cloud};

// ====
// BUSH
// ====

static void load_bush() {
	load_texture_num("props/bush%u", 3L);
}

static void draw_bush(const GameActor* actor) {
	const char* tex = NULL;
	switch (((int)((float)game_state.time / 7.142857142857143f) + VAL(actor, PROP_FRAME)) % 4L) {
	default:
		tex = "props/bush0";
		break;
	case 1L:
	case 3L:
		tex = "props/bush1";
		break;
	case 2L:
		tex = "props/bush2";
		break;
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_BUSH = {.load = load_bush, .create = create, .draw = draw_bush};
