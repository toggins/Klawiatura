#include "K_game.h"

enum {
	VAL_PROP_FRAME = VAL_CUSTOM,
};

enum {
	FLG_PROP_EXTRA = CUSTOM_FLAG(0),
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

// ================
// GIANT CLOUD FACE
// ================

static void load_face() {
	load_texture_num("bg/cloud_face%u", 10L);
}

static void create_face(GameActor* actor) {
	actor->box.end.x = FfInt(30L);
	actor->box.end.y = FfInt(32L);
}

static void tick_face(GameActor* actor) {
	if (VAL(actor, PROP_FRAME) > 0L) {
		if (ANY_FLAG(actor, FLG_PROP_EXTRA)) {
			VAL(actor, PROP_FRAME) += 4L;
			if (VAL(actor, PROP_FRAME) > 200L)
				VAL(actor, PROP_FRAME) = 0L;
		} else {
			VAL(actor, PROP_FRAME) += 100L;
			if (VAL(actor, PROP_FRAME) > 1400L)
				VAL(actor, PROP_FRAME) = 0L;
		}
		return;
	} else if ((game_state.time % 5L) != 0L)
		return;

	switch (rng(20L)) {
	default:
		break;

	case 10L: {
		VAL(actor, PROP_FRAME) = 1L;
		FLAG_OFF(actor, FLG_PROP_EXTRA);
		break;
	}

	case 15L: {
		VAL(actor, PROP_FRAME) = 1L;
		FLAG_ON(actor, FLG_PROP_EXTRA);
		break;
	}
	}
}

static void draw_face(const GameActor* actor) {
	const char* tex = "bg/cloud_face0";
	const uint8_t frame = VAL(actor, PROP_FRAME) / 100L;
	if (frame > 0L) {
		if (ANY_FLAG(actor, FLG_PROP_EXTRA))
			tex = "bg/cloud_face9";
		else
			switch (frame) {
			default:
				tex = "bg/cloud_face1";
				break;
			case 2L:
			case 14L:
				tex = "bg/cloud_face2";
				break;
			case 3L:
			case 13L:
				tex = "bg/cloud_face3";
				break;
			case 4L:
			case 12L:
				tex = "bg/cloud_face4";
				break;
			case 5L:
			case 11L:
				tex = "bg/cloud_face5";
				break;
			case 6L:
			case 10L:
				tex = "bg/cloud_face6";
				break;
			case 7L:
			case 9L:
				tex = "bg/cloud_face7";
				break;
			case 8L:
				tex = "bg/cloud_face8";
				break;
			}
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_CLOUD_FACE = {.load = load_face, .create = create_face, .tick = tick_face, .draw = draw_face};
