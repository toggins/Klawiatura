#include "K_game.h"
#include "K_string.h"

#include "actors/K_autoscroll.h"

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FxFrom(32L);
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
	load_texture_num("tiles/wheel_l%u", 3L);
}

static void load_wheel(GameActor* actor) {
	load_texture_num("tiles/wheel%u", 4L);
}

static void load_right_wheel(GameActor* actor) {
	load_texture_num("tiles/wheel_r%u", 3L);
}

static void tick_wheel(GameActor* actor) {
	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL && ANY_FLAG(autoscroll, FLG_SCROLL_TANKS) && VAL(autoscroll, X_SPEED) != FxZero)
		VAL(actor, WHEEL_FRAME) += (autoscroll->values[VAL_X_SPEED] > FxZero) ? FxHalf : -FxHalf;
}

static void draw_left_wheel(const GameActor* actor) {
	draw_actor(actor, fmt("tiles/wheel_l%u", Fx2Int(VAL(actor, WHEEL_FRAME)) % 3L), 0.f, B_WHITE);
}

static void draw_wheel(const GameActor* actor) {
	batch_reset();
	const InterpActor* iactor = get_interp(actor);
	batch_pos(B_XYZ(Fx2Int(iactor->pos.x), Fx2Int(iactor->pos.y), Fx2Float(actor->depth)));
	batch_tile(B_TILE(TRUE, FALSE));
	batch_rectangle(fmt("tiles/wheel%u", Fx2Int(VAL(actor, WHEEL_FRAME)) % 4L),
		B_WH(Fx2Int(actor->box.end.x - actor->box.start.x), Fx2Int(actor->box.end.y - actor->box.start.y)));
}

static void draw_right_wheel(const GameActor* actor) {
	draw_actor(actor, fmt("tiles/wheel_r%u", Fx2Int(VAL(actor, WHEEL_FRAME)) % 3L), 0.f, B_WHITE);
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
