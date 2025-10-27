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
	load_texture("props/cloud");
	load_texture("props/cloud2");
	load_texture("props/cloud3");
}

static void draw_cloud(const GameActor* actor) {
	const char* tex;
	switch (((int)((float)game_state.time / 12.5f) + VAL(actor, PROP_FRAME)) % 4L) {
	default:
		tex = "props/cloud";
		break;
	case 1L:
	case 3L:
		tex = "props/cloud2";
		break;
	case 2L:
		tex = "props/cloud3";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

const GameActorTable TAB_CLOUD = {.load = load_cloud, .create = create, .draw = draw_cloud};

// ====
// BUSH
// ====

static void load_bush() {
	load_texture("props/bush");
	load_texture("props/bush2");
	load_texture("props/bush3");
}

static void draw_bush(const GameActor* actor) {
	const char* tex;
	switch (((int)((float)game_state.time / 7.142857142857143f) + VAL(actor, PROP_FRAME)) % 4L) {
	default:
		tex = "props/bush";
		break;
	case 1L:
	case 3L:
		tex = "props/bush2";
		break;
	case 2L:
		tex = "props/bush3";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

const GameActorTable TAB_BUSH = {.load = load_bush, .create = create, .draw = draw_bush};
