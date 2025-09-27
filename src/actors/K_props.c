#include "K_game.h"

enum {
	VAL_PROP_FRAME,
};

static void create(GameActor* actor) {
	actor->values[VAL_PROP_FRAME] = rng(100);
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
	switch (((int)((float)game_state.time / 12.5f) + actor->values[VAL_PROP_FRAME]) % 4) {
	default:
		tex = "props/cloud";
		break;
	case 1:
	case 3:
		tex = "props/cloud2";
		break;
	case 2:
		tex = "props/cloud3";
		break;
	}
	draw_actor(actor, tex, 0, WHITE);
}

const GameActorTable TAB_CLOUD = {SOL_NONE, .load = load_cloud, .create = create, .draw = draw_cloud};

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
	switch (((int)((float)game_state.time / 7.142857142857143f) + actor->values[VAL_PROP_FRAME]) % 4) {
	default:
		tex = "props/bush";
		break;
	case 1:
	case 3:
		tex = "props/bush2";
		break;
	case 2:
		tex = "props/bush3";
		break;
	}
	draw_actor(actor, tex, 0, WHITE);
}

const GameActorTable TAB_BUSH = {SOL_NONE, .load = load_bush, .create = create, .draw = draw_bush};
