#include "K_game.h"
#include "K_string.h"

#include "actors/K_block.h"

enum {
	VAL_EFFECT_FRAME = VAL_CUSTOM,
};

enum {
	FLG_EFFECT_POP = CUSTOM_FLAG(0),
};

// =========
// EXPLOSION
// =========

static void load_explode() {
	load_texture_num("effects/explode%u", 3L, FALSE);
}

static void tick_explode(GameActor* actor) {
	VAL(actor, EFFECT_FRAME) += 24L;
	if (VAL(actor, EFFECT_FRAME) >= 300L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_explode(const GameActor* actor) {
	const char* tex = fmt("effects/explode%u", VAL(actor, EFFECT_FRAME) / 100L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_EXPLODE = {.load = load_explode, .tick = tick_explode, .draw = draw_explode};

// ===========
// BRICK SHARD
// ===========

static void load_shard() {
	load_texture("effects/shard", FALSE);
	load_texture("effects/shard_gray", FALSE);
}

static void create_shard(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-8L);
	actor->box.end.x = actor->box.end.y = FfInt(8L);
}

static void tick_shard(GameActor* actor) {
	if (!in_any_view(actor, FxZero, TRUE)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, BRICK_SHARD_ANGLE) += 28824L;
	VAL(actor, Y_SPEED) += 26214L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_shard(const GameActor* actor) {
	draw_actor(actor, ANY_FLAG(actor, FLG_BLOCK_GRAY) ? "effects/shard_gray" : "effects/shard",
		FtFloat(VAL(actor, BRICK_SHARD_ANGLE)), B_WHITE);
}

const GameActorTable TAB_BRICK_SHARD = {
	.load = load_shard,
	.create = create_shard,
	.tick = tick_shard,
	.draw = draw_shard,
};

// ============
// WATER SPLASH
// ============

static void load_splash() {
	load_texture_num("effects/water%u", 15L, FALSE);
}

static void create_splash(GameActor* actor) {
	actor->depth = FfInt(-2L);
}

static void tick_splash(GameActor* actor) {
	VAL(actor, EFFECT_FRAME) += 7L;
	if (VAL(actor, EFFECT_FRAME) >= 150L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_splash(const GameActor* actor) {
	const char* tex = fmt("effects/water%u", VAL(actor, EFFECT_FRAME) / 10L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

const GameActorTable TAB_WATER_SPLASH = {
	.load = load_splash,
	.create = create_splash,
	.tick = tick_splash,
	.draw = draw_splash,
};

// ======
// BUBBLE
// ======

static void load_bubble() {
	load_texture_num("effects/bubble%u", 8L, FALSE);
}

static void create_bubble(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-4L);
	actor->box.end.x = FfInt(5L);
	actor->box.end.y = FfInt(6L);
}

static void tick_bubble(GameActor* actor) {
	if (!in_any_view(actor, FfInt(28L), FALSE)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	++VAL(actor, EFFECT_FRAME);

	if (ANY_FLAG(actor, FLG_EFFECT_POP)) {
		if (VAL(actor, EFFECT_FRAME) >= 7L)
			FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	const Fixed xoffs = FfInt(-2L + rng(5L));
	const Fixed yoffs = FfInt(-rng(3L));
	move_actor(actor, POS_ADD(actor, xoffs, yoffs));
	if (actor->pos.y < game_state.water) {
		VAL(actor, EFFECT_FRAME) = 0L;
		FLAG_ON(actor, FLG_EFFECT_POP);
	}
}

static void draw_bubble(const GameActor* actor) {
	float pos[2] = {0.f};

	const char* tex = "effects/bubble0";
	if (ANY_FLAG(actor, FLG_EFFECT_POP))
		tex = fmt("effects/bubble%u", 1L + VAL(actor, EFFECT_FRAME));
	else
		switch ((VAL(actor, EFFECT_FRAME) / 2L) % 5L) {
		default:
			break;
		case 1L:
			pos[0] -= 1.f;
			break;
		case 2L:
			pos[0] += 1.f;
			break;
		case 4L:
			pos[1] -= 2.f;
			break;
		}

	const InterpActor* iactor = get_interp(actor);
	batch_reset();
	batch_pos(B_XYZ(FtInt(iactor->pos.x) + pos[0], FtInt(iactor->pos.y) + pos[1], FtFloat(actor->depth)));
	batch_sprite(tex);
}

const GameActorTable TAB_BUBBLE = {
	.load = load_bubble,
	.create = create_bubble,
	.tick = tick_bubble,
	.draw = draw_bubble,
};

// ===========
// LAVA SPLASH
// ===========

static void load_lava() {
	load_texture_num("effects/lava%u", 11L, FALSE);
}

static void tick_lava(GameActor* actor) {
	VAL(actor, EFFECT_FRAME) += 49L;
	if (VAL(actor, EFFECT_FRAME) >= 1100L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_lava(const GameActor* actor) {
	draw_actor(actor, fmt("effects/lava%u", VAL(actor, EFFECT_FRAME) / 100L), 0.f, B_WHITE);
}

const GameActorTable TAB_LAVA_SPLASH = {
	.load = load_lava,
	.tick = tick_lava,
	.draw = draw_lava,
};

// ===========
// LAVA BUBBLE
// ===========

static void load_lava_bubble() {
	load_texture("effects/lava_bubble", FALSE);
}

static void create_lava_bubble(GameActor* actor) {
	actor->box.start.x = FfInt(-7L);
	actor->box.start.y = FfInt(-6L);
	actor->box.end.x = FfInt(8L);
	actor->box.end.y = FfInt(9L);
}

static void tick_lava_bubble(GameActor* actor) {
	if (++VAL(actor, EFFECT_FRAME) >= 42L || !in_any_view(actor, FxZero, FALSE))
		FLAG_ON(actor, FLG_DESTROY);
	VAL(actor, Y_SPEED) += 6554L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_lava_bubble(const GameActor* actor) {
	const InterpActor* iactor = get_interp(actor);
	batch_reset(), batch_pos(B_XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y), FtFloat(actor->depth)));
	const float scale = 1.f - ((float)VAL(actor, EFFECT_FRAME) / 42.f);
	batch_scale(B_SCALE(scale));
	batch_sprite("effects/lava_bubble");
}

const GameActorTable TAB_LAVA_BUBBLE = {
	.load = load_lava_bubble,
	.create = create_lava_bubble,
	.tick = tick_lava_bubble,
	.draw = draw_lava_bubble,
};

// ===========
// TUBE BUBBLE
// ===========

static void load_tube_bubble() {
	load_texture("effects/bubble0", FALSE);
}

static void tick_tube_bubble(GameActor* actor) {
	if (!in_any_view(actor, FxZero, FALSE)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	++VAL(actor, EFFECT_FRAME);
	move_actor(actor, POS_SPEED(actor));
}

static void draw_tube_bubble(const GameActor* actor) {
	float pos[2] = {0.f};
	switch ((VAL(actor, EFFECT_FRAME) / 2L) % 5L) {
	default:
		break;
	case 1L:
		pos[0] -= 1.f;
		break;
	case 2L:
		pos[0] += 1.f;
		break;
	case 4L:
		pos[1] -= 2.f;
		break;
	}

	const InterpActor* iactor = get_interp(actor);
	batch_reset();
	batch_pos(B_XYZ(FtInt(iactor->pos.x) + pos[0], FtInt(iactor->pos.y) + pos[1], FtFloat(actor->depth)));
	batch_color(B_ALPHA(57L));
	batch_sprite("effects/bubble0");
}

const GameActorTable TAB_TUBE_BUBBLE = {
	.load = load_tube_bubble,
	.create = create_bubble,
	.tick = tick_tube_bubble,
	.draw = draw_tube_bubble,
};
