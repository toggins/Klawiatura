#include "actors/K_player.h"

enum {
	SPIKE_NORMAL,
	SPIKE_CASTLE,
	SPIKE_TANK,
	SPIKE_TANK_LEFT,
};

enum {
	VAL_SPIKE_TYPE = VAL_CUSTOM,
};

enum {
	FLG_SPIKE_START = CUSTOM_FLAG(0),
};

// =====
// SPIKE
// =====

static void load_special_spike(const GameActor* actor) {
	switch (VAL(actor, SPIKE_TYPE)) {
	default:
		load_texture("enemies/spike");
		break;
	case SPIKE_CASTLE:
		load_texture("enemies/spike_castle");
		break;
	case SPIKE_TANK:
		load_texture("enemies/spike_tank");
		break;
	case SPIKE_TANK_LEFT:
		load_texture("enemies/spike_tank_l");
		break;
	}
}

static void create_spike(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FxOne;
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(32L);
}

static void tick_spike(GameActor* actor) {
	if (ANY_FLAG(actor, FLG_SPIKE_START))
		return;

	if (ANY_FLAG(actor, FLG_Y_FLIP)) {
		actor->box.start.y = FfInt(-32L);
		actor->box.end.y = -FxOne;
	}
	FLAG_ON(actor, FLG_SPIKE_START);
}

static void draw_spike(const GameActor* actor) {
	const char* tex = "enemies/spike";
	switch (VAL(actor, SPIKE_TYPE)) {
	default:
		break;
	case SPIKE_CASTLE:
		tex = "enemies/spike_castle";
		break;
	case SPIKE_TANK:
		tex = "enemies/spike_tank";
		break;
	case SPIKE_TANK_LEFT:
		tex = "enemies/spike_tank_l";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide_spike(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER)
		hit_player(from);
}

const GameActorTable TAB_SPIKE = {
	.load_special = load_special_spike,
	.create = create_spike,
	.tick = tick_spike,
	.draw = draw_spike,
	.collide = collide_spike,
};

// ==============
// ELECTRIC CORAL
// ==============

static void load_coral() {
	load_texture_wild("enemies/coral?");
}

static void create_coral(GameActor* actor) {
	actor->box.end.x = FfInt(28L);
	actor->box.end.y = FfInt(30L);

	actor->depth = FfInt(20L);
}

static void draw_coral(const GameActor* actor) {
	const char* tex = "enemies/coral7";
	switch ((game_state.time / 2L) % 22L) {
	default:
		break;
	case 0L:
		tex = "enemies/coral";
		break;
	case 1L:
		tex = "enemies/coral2";
		break;
	case 2L:
		tex = "enemies/coral3";
		break;
	case 3L:
		tex = "enemies/coral4";
		break;
	case 4L:
		tex = "enemies/coral5";
		break;
	case 5L:
		tex = "enemies/coral6";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void on_coral(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER)
		hit_player(from);
}

const GameActorTable TAB_ELECTRIC_CORAL = {
	.is_solid = always_solid,
	.load = load_coral,
	.create = create_coral,
	.draw = draw_coral,
	.on_top = on_coral,
	.on_bottom = on_coral,
	.on_left = on_coral,
	.on_right = on_coral,
};
