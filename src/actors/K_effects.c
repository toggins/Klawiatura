#include "K_game.h"

#include "actors/K_block.h"

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
	VAL(actor, EXPLODE_FRAME) += 24L;
	if (VAL(actor, EXPLODE_FRAME) >= 300L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_explode(const GameActor* actor) {
	const char* tex;
	switch (VAL(actor, EXPLODE_FRAME) / 100L) {
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

const GameActorTable TAB_BRICK_SHARD
	= {.load = load_shard, .create = create_shard, .tick = tick_shard, .draw = draw_shard};
