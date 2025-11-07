#include "K_game.h"

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
	load_texture_wild("effects/explode?");
}

static void tick_explode(GameActor* actor) {
	VAL(actor, EFFECT_FRAME) += 24L;
	if (VAL(actor, EFFECT_FRAME) >= 300L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_explode(const GameActor* actor) {
	const char* tex = NULL;
	switch (VAL(actor, EFFECT_FRAME) / 100L) {
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

// ===========
// BRICK SHARD
// ===========

static void load_shard() {
	load_texture("effects/shard");
	load_texture("effects/shard_gray");
}

static void create_shard(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-8L);
	actor->box.end.x = actor->box.end.y = FfInt(8L);
}

static void tick_shard(GameActor* actor) {
	if (!in_any_view(actor, FxZero, true)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, BRICK_SHARD_ANGLE) += 28824L;
	VAL(actor, Y_SPEED) += 26214L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_shard(const GameActor* actor) {
	draw_actor(actor, ANY_FLAG(actor, FLG_BLOCK_GRAY) ? "effects/shard_gray" : "effects/shard",
		FtFloat(VAL(actor, BRICK_SHARD_ANGLE)), WHITE);
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
	load_texture_wild("effects/water??");
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
	const char* tex = NULL;
	switch (VAL(actor, EFFECT_FRAME) / 10L) {
	default:
		tex = "effects/water";
		break;
	case 1L:
		tex = "effects/water2";
		break;
	case 2L:
		tex = "effects/water3";
		break;
	case 3L:
		tex = "effects/water4";
		break;
	case 4L:
		tex = "effects/water5";
		break;
	case 5L:
		tex = "effects/water6";
		break;
	case 6L:
		tex = "effects/water7";
		break;
	case 7L:
		tex = "effects/water8";
		break;
	case 8L:
		tex = "effects/water9";
		break;
	case 9L:
		tex = "effects/water10";
		break;
	case 10L:
		tex = "effects/water11";
		break;
	case 11L:
		tex = "effects/water12";
		break;
	case 12L:
		tex = "effects/water13";
		break;
	case 13L:
		tex = "effects/water14";
		break;
	case 14L:
		tex = "effects/water15";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
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
	load_texture_wild("effects/bubble?");
}

static void create_bubble(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-4L);
	actor->box.end.x = FfInt(5L);
	actor->box.end.y = FfInt(6L);
}

static void tick_bubble(GameActor* actor) {
	if (!in_any_view(actor, FfInt(28L), false)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	++VAL(actor, EFFECT_FRAME);

	if (ANY_FLAG(actor, FLG_EFFECT_POP)) {
		if (VAL(actor, EFFECT_FRAME) >= 7L)
			FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	const fixed xoffs = FfInt(-2L + rng(5L));
	const fixed yoffs = FfInt(-rng(3L));
	move_actor(actor, POS_ADD(actor, xoffs, yoffs));
	if (actor->pos.y < game_state.water) {
		VAL(actor, EFFECT_FRAME) = 0L;
		FLAG_ON(actor, FLG_EFFECT_POP);
	}
}

static void draw_bubble(const GameActor* actor) {
	float pos[2] = {0.f};

	const char* tex = NULL;
	if (ANY_FLAG(actor, FLG_EFFECT_POP)) {
		switch (VAL(actor, EFFECT_FRAME)) {
		default:
			tex = "effects/bubble2";
			break;
		case 1L:
			tex = "effects/bubble3";
			break;
		case 2L:
			tex = "effects/bubble4";
			break;
		case 3L:
			tex = "effects/bubble5";
			break;
		case 4L:
			tex = "effects/bubble6";
			break;
		case 5L:
			tex = "effects/bubble7";
			break;
		case 6L:
			tex = "effects/bubble8";
			break;
		}
	} else {
		tex = "effects/bubble";
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
	}

	const InterpActor* iactor = get_interp(actor);
	batch_start(
		XYZ(FtInt(iactor->pos.x) + pos[0], FtInt(iactor->pos.y) + pos[1], FtFloat(actor->depth)), 0.f, WHITE);
	batch_sprite(tex, NO_FLIP);
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
	load_texture_wild("effects/lava??");
}

static void tick_lava(GameActor* actor) {
	VAL(actor, EFFECT_FRAME) += 49L;
	if (VAL(actor, EFFECT_FRAME) >= 1100L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_lava(const GameActor* actor) {
	const char* tex = NULL;
	switch (VAL(actor, EFFECT_FRAME) / 100L) {
	default:
		tex = "effects/lava";
		break;
	case 1L:
		tex = "effects/lava2";
		break;
	case 2L:
		tex = "effects/lava3";
		break;
	case 3L:
		tex = "effects/lava4";
		break;
	case 4L:
		tex = "effects/lava5";
		break;
	case 5L:
		tex = "effects/lava6";
		break;
	case 6L:
		tex = "effects/lava7";
		break;
	case 7L:
		tex = "effects/lava8";
		break;
	case 8L:
		tex = "effects/lava9";
		break;
	case 9L:
		tex = "effects/lava10";
		break;
	case 10L:
		tex = "effects/lava11";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

const GameActorTable TAB_LAVA_SPLASH = {
	.load = load_lava,
	.tick = tick_lava,
	.draw = draw_lava,
};
