#include "K_game.h"
#include "K_string.h"
#include "K_tick.h"

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
	load_texture_num("props/cloud%u", 3L, FALSE);
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
	load_texture_num("props/bush%u", 3L, FALSE);
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
	load_texture_num("bg/cloud_face%u", 10L, FALSE);
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
	const Uint8 frame = VAL(actor, PROP_FRAME) / 100L;
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

// =============
// MOVING CLOUDS
// =============

static void load_clouds() {
	load_texture("bg/clouds", FALSE);
}

static void draw_clouds(const GameActor* actor) {
	batch_reset();
	batch_pos(B_XYZ(-64.f + SDL_floorf(SDL_fmodf(totalticks() * FtFloat(VAL(actor, X_SPEED)), 64.f)),
		FtInt(actor->pos.y), FtFloat(actor->depth)));
	batch_tile(B_TILE(TRUE, FALSE));
	batch_rectangle("bg/clouds", B_XY(FtFloat(game_state.size.x) + 128.f, 64.f));
}

const GameActorTable TAB_CLOUDS = {.load = load_clouds, .draw = draw_clouds};

// ==========
// LAMP LIGHT
// ==========

static void load_lamp_light() {
	load_texture_num("effects/lamp_light%u", 4L, FALSE);
}

static void draw_lamp_light(const GameActor* actor) {
	const char* tex = NULL;
	switch (((int)((float)game_state.time / 2.5f) + VAL(actor, PROP_FRAME)) % 6L) {
	default:
		tex = "effects/lamp_light0";
		break;
	case 1L:
	case 5L:
		tex = "effects/lamp_light1";
		break;
	case 2L:
	case 4L:
		tex = "effects/lamp_light2";
		break;
	case 3L:
		tex = "effects/lamp_light3";
		break;
	}

	batch_reset();
	const InterpActor* iactor = get_interp(actor);
	batch_pos(B_XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y), FtFloat(actor->depth)));
	batch_color(B_ALPHA(155L)), batch_blend(B_BLEND_ADD);
	batch_sprite(tex);
	batch_blend(B_BLEND_NORMAL);
}

const GameActorTable TAB_LAMP_LIGHT = {.load = load_lamp_light, .create = create, .draw = draw_lamp_light};

// ===========
// BUBBLE TUBE
// ===========

static void load_tube() {
	load_actor(ACT_TUBE_BUBBLE);
}

static void create_tube(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick_tube(GameActor* actor) {
	if ((game_state.time % 5L) != 0L || !in_any_view(actor, FxZero, FALSE) || rng(11L) != 10L)
		return;
	GameActor* bubble = create_actor(ACT_TUBE_BUBBLE, POS_ADD(actor, FfInt(9L + rng(16L)), -FxOne));
	if (bubble == NULL)
		return;
	bubble->depth = actor->depth + FxOne;
	VAL(bubble, Y_SPEED) = (1L + rng(60L)) * -8192L;
}

const GameActorTable TAB_BUBBLE_TUBE = {.load = load_tube, .create = create_tube, .tick = tick_tube};

// =========
// WATERFALL
// =========

static void load_waterfall() {
	load_texture_num("bg/waterfall%u", 4L, FALSE);
}

static void draw_waterfall(const GameActor* actor) {
	const char* tex = fmt(
		"bg/waterfall%u", ((int)((float)game_state.time / 2.439024390243902f) + VAL(actor, PROP_FRAME)) % 4L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_WATERFALL = {.load = load_waterfall, .create = create, .draw = draw_waterfall};

// ========
// LAVAFALL
// ========

static void load_lavafall() {
	load_texture_num("bg/lavafall%u", 6L, FALSE);
}

static void draw_lavafall(const GameActor* actor) {
	const float tt = totalticks();
	const char* tex = NULL;
	switch ((int)tt % 10L) {
	default:
		tex = "bg/lavafall0";
		break;
	case 1L:
	case 9L:
		tex = "bg/lavafall1";
		break;
	case 2L:
	case 8L:
		tex = "bg/lavafall2";
		break;
	case 3L:
	case 7L:
		tex = "bg/lavafall3";
		break;
	case 4L:
	case 6L:
		tex = "bg/lavafall4";
		break;
	case 5L:
		tex = "bg/lavafall5";
		break;
	}

	batch_reset();
	batch_pos(B_XYZ(FtInt(actor->pos.x), -32.f + SDL_fmodf(((float)VAL(actor, PROP_FRAME) + tt) * 6.25f, 32.f),
		FtFloat(actor->depth)));
	batch_tile(B_TILE(FALSE, TRUE)), batch_rectangle(tex, B_XY(54.f, FtFloat(game_state.size.y) + 32.f));
}

const GameActorTable TAB_LAVAFALL = {.load = load_lavafall, .create = create, .draw = draw_lavafall};

// ===================
// LAVA BUBBLE SPAWNER
// ===================

static void load_laver() {
	load_actor(ACT_LAVA_BUBBLE);
}

static void create_laver(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick_laver(GameActor* actor) {
	if (((game_state.time * 2L) % 5L) >= 2L || !in_any_view(actor, FxZero, FALSE))
		return;

	GameActor* bubble = create_actor(ACT_LAVA_BUBBLE, POS_ADD(actor, FfInt(-7L + rng(48L)), FxZero));
	if (bubble == NULL)
		return;
	VAL(bubble, X_SPEED) = FfInt(-2L + rng(5L));
	VAL(bubble, Y_SPEED) = FfInt(-2L - rng(4L));
}

const GameActorTable TAB_LAVA_BUBBLE_SPAWNER = {.load = load_laver, .create = create_laver, .tick = tick_laver};
