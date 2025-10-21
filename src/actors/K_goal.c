#include "K_game.h"
#include "K_player.h"
#include "K_points.h"

enum {
	VAL_GOAL_Y = VAL_CUSTOM,
	VAL_GOAL_ANGLE,
};

enum {
	FLG_GOAL_START = CUSTOM_FLAG(0),
};

// =========
// GOAL MARK
// =========

static void load_mark() {
	load_actor(ACT_POINTS);
}

static void create_mark(GameActor* actor) {
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(32L);
}

static void collide_mark(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || from->values[VAL_PLAYER_GROUND] <= 0L || game_state.sequence.type == SEQ_WIN)
		return;
	give_points(from, get_owner(from), 100L);
	win_player(from);
}

const GameActorTable TAB_GOAL_MARK = {.load = load_mark, .create = create_mark, .collide = collide_mark};

// ========
// GOAL BAR
// ========

static void load_bar() {
	load_texture("markers/goal_bar");

	load_actor(ACT_GOAL_BAR_FLY);
	load_actor(ACT_POINTS);
}

static void create_bar(GameActor* actor) {
	actor->box.start.x = FfInt(-23L);
	actor->box.end.x = FfInt(21L);
	actor->box.end.y = FfInt(16L);
}

static void tick_bar(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_GOAL_START)) {
		actor->values[VAL_GOAL_Y] = actor->pos.y;
		actor->values[VAL_Y_SPEED] = FfInt(3L);
		FLAG_ON(actor, FLG_GOAL_START);
	}

	if (game_state.sequence.type == SEQ_WIN)
		actor->values[VAL_Y_SPEED] = FxZero;

	if ((actor->values[VAL_Y_SPEED] > FxZero && actor->pos.y >= (actor->values[VAL_GOAL_Y] + FfInt(220L)))
		|| (actor->values[VAL_Y_SPEED] < FxZero && actor->pos.y <= actor->values[VAL_GOAL_Y]))
		actor->values[VAL_Y_SPEED] = -actor->values[VAL_Y_SPEED];
	move_actor(actor, POS_SPEED(actor));
}

static void draw_bar(const GameActor* actor) {
	draw_actor(actor, "markers/goal_bar", 0.f, WHITE);
}

static void collide_bar(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || game_state.sequence.type == SEQ_WIN)
		return;

	int32_t points;
	if (actor->pos.y < (actor->values[VAL_GOAL_Y] + FfInt(30L)))
		points = 10000L;
	else if (actor->pos.y < (actor->values[VAL_GOAL_Y] + FfInt(60L)))
		points = 5000L;
	else if (actor->pos.y < (actor->values[VAL_GOAL_Y] + FfInt(100L)))
		points = 2000L;
	else if (actor->pos.y < (actor->values[VAL_GOAL_Y] + FfInt(150L)))
		points = 1000L;
	else if (actor->pos.y < (actor->values[VAL_GOAL_Y] + FfInt(200L)))
		points = 500L;
	else
		points = 200L;
	give_points(actor, get_owner(from), points);
	win_player(from);

	GameActor* bar = create_actor(ACT_GOAL_BAR_FLY, POS_ADD(actor, FfInt(-2L), FfInt(7L)));
	if (bar != NULL) {
		const fixed dir = 154416L + (-12868L + Fmul(FfInt(rng(3L)), 12868L));
		bar->values[VAL_X_SPEED] = Fmul(327680L, Fcos(dir));
		bar->values[VAL_Y_SPEED] = Fmul(327680L, -Fsin(dir));
		FLAG_ON(actor, FLG_DESTROY);
	}
}

const GameActorTable TAB_GOAL_BAR
	= {.load = load_bar, .create = create_bar, .tick = tick_bar, .draw = draw_bar, .collide = collide_bar};

// ===============
// FLYING GOAL BAR
// ===============

static void load_fly() {
	load_texture("markers/goal_bar2");
}

static void create_fly(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-23L);
	actor->box.end.x = actor->box.end.y = FfInt(23L);
}

static void tick_fly(GameActor* actor) {
	if (actor->pos.y > game_state.size.y) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	actor->values[VAL_GOAL_ANGLE] += 25736L;
	actor->values[VAL_Y_SPEED] += 13107L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_fly(const GameActor* actor) {
	draw_actor(actor, "markers/goal_bar2", FtFloat(actor->values[VAL_GOAL_ANGLE]), WHITE);
}

const GameActorTable TAB_GOAL_BAR_FLY = {.load = load_fly, .create = create_fly, .tick = tick_fly, .draw = draw_fly};
