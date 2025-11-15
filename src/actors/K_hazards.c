#include "K_string.h"

#include "actors/K_player.h"

enum {
	SPIKE_NORMAL,
	SPIKE_CASTLE,
	SPIKE_TANK,
	SPIKE_TANK_LEFT,
};

enum {
	VAL_SPIKE_TYPE = VAL_CUSTOM,

	VAL_CLOUD_FRAME = VAL_CUSTOM,
};

enum {
	FLG_SPIKE_START = CUSTOM_FLAG(0),

	FLG_CLOUD_HIDDEN = CUSTOM_FLAG(0),
	FLG_CLOUD_TRANSLUCENT = CUSTOM_FLAG(1),
	FLG_CLOUD_TROLL = CUSTOM_FLAG(2),
	FLG_CLOUD_FRIENDLY = CUSTOM_FLAG(3),
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
	const uint64_t frame = (game_state.time / 2L) % 22L;
	const char* tex = fmt("enemies/coral%s", frame >= 6 ? "7" : txnum(frame));
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

// ==========
// FAKE CLOUD
// ==========

static void load_cloud() {
	load_texture_wild("enemies/fake_cloud?");
	load_texture_wild("props/cloud?");

	load_sound("fake_cloud");
}

static void create_cloud(GameActor* actor) {
	actor->box.start.x = FxOne;
	actor->box.end.x = FfInt(64L);
	actor->box.end.y = FfInt(48L);

	VAL(actor, CLOUD_FRAME) = rng(3L);
}

static void draw_cloud(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_CLOUD_HIDDEN))
		return;
	const int frame = ((int)((float)game_state.time / 12.5f) + VAL(actor, CLOUD_FRAME)) % 3L;
	const char* tex
		= fmt(ANY_FLAG(actor, FLG_CLOUD_TROLL) ? "props/cloud%s" : "enemies/fake_cloud%s", txnum(frame));
	draw_actor(actor, tex, 0.f, ALPHA(ANY_FLAG(actor, FLG_CLOUD_TRANSLUCENT) ? 64L : 255L));
}

static void collide_cloud(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_CLOUD_FRIENDLY))
		return;
	kill_player(from);
	FLAG_OFF(actor, FLG_CLOUD_HIDDEN | FLG_CLOUD_TRANSLUCENT | FLG_CLOUD_TROLL);
	play_actor_sound(actor, "fake_cloud");
}

const GameActorTable TAB_FAKE_CLOUD = {
	.load = load_cloud,
	.create = create_cloud,
	.draw = draw_cloud,
	.collide = collide_cloud,
};
