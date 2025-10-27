#include "K_game.h"

#include "actors/K_autoscroll.h"

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

const GameActorTable TAB_SOLID = {.is_solid = always_solid, .create = create},
		     TAB_SOLID_TOP = {.is_solid = always_top, .create = create},
		     TAB_SOLID_SLOPE = {.is_solid = always_solid, .create = create};

// ===========
// TANK WHEELS
// ===========

enum {
	VAL_WHEEL_FRAME = VAL_CUSTOM,
};

static void load_left_wheel(GameActor* actor) {
	load_texture("tiles/wheel_l");
	load_texture("tiles/wheel_l2");
	load_texture("tiles/wheel_l3");
}

static void load_wheel(GameActor* actor) {
	load_texture("tiles/wheel");
	load_texture("tiles/wheel2");
	load_texture("tiles/wheel3");
	load_texture("tiles/wheel4");
}

static void load_right_wheel(GameActor* actor) {
	load_texture("tiles/wheel_r");
	load_texture("tiles/wheel_r2");
	load_texture("tiles/wheel_r3");
}

static void tick_wheel(GameActor* actor) {
	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL && ANY_FLAG(autoscroll, FLG_SCROLL_TANKS) && VAL(autoscroll, X_SPEED) != FxZero)
		VAL(actor, WHEEL_FRAME) += (autoscroll->values[VAL_X_SPEED] > FxZero) ? FxHalf : -FxHalf;
}

static void draw_left_wheel(const GameActor* actor) {
	const char* tex;
	switch (FtInt(VAL(actor, WHEEL_FRAME)) % 3L) {
	default:
		tex = "tiles/wheel_l";
		break;
	case 1L:
		tex = "tiles/wheel_l2";
		break;
	case 2L:
		tex = "tiles/wheel_l3";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

static void draw_wheel(const GameActor* actor) {
	const char* tex;
	switch (FtInt(VAL(actor, WHEEL_FRAME)) % 4L) {
	default:
		tex = "tiles/wheel";
		break;
	case 1L:
		tex = "tiles/wheel2";
		break;
	case 2L:
		tex = "tiles/wheel3";
		break;
	case 3L:
		tex = "tiles/wheel4";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

static void draw_right_wheel(const GameActor* actor) {
	const char* tex;
	switch (FtInt(VAL(actor, WHEEL_FRAME)) % 3L) {
	default:
		tex = "tiles/wheel_r";
		break;
	case 1L:
		tex = "tiles/wheel_r2";
		break;
	case 2L:
		tex = "tiles/wheel_r3";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

const GameActorTable TAB_WHEEL_LEFT = {
	.is_solid = always_solid,
	.load = load_left_wheel,
	.create = create,
	.tick = tick_wheel,
	.draw = draw_left_wheel,
};
const GameActorTable TAB_WHEEL = {
	.is_solid = always_solid,
	.load = load_wheel,
	.create = create,
	.tick = tick_wheel,
	.draw = draw_wheel,
};
const GameActorTable TAB_WHEEL_RIGHT = {
	.is_solid = always_solid,
	.load = load_right_wheel,
	.create = create,
	.tick = tick_wheel,
	.draw = draw_right_wheel,
};
