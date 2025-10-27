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
	if (from->type != ACT_PLAYER || VAL(from, PLAYER_GROUND) <= 0L || game_state.sequence.type == SEQ_WIN)
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
		VAL(actor, GOAL_Y) = actor->pos.y;
		VAL(actor, Y_SPEED) = FfInt(3L);
		FLAG_ON(actor, FLG_GOAL_START);
	}

	if (game_state.sequence.type == SEQ_WIN)
		VAL(actor, Y_SPEED) = FxZero;

	if ((VAL(actor, Y_SPEED) > FxZero && actor->pos.y >= (VAL(actor, GOAL_Y) + FfInt(220L)))
		|| (VAL(actor, Y_SPEED) < FxZero && actor->pos.y <= VAL(actor, GOAL_Y)))
		VAL(actor, Y_SPEED) = -VAL(actor, Y_SPEED);
	move_actor(actor, POS_SPEED(actor));
}

static void draw_bar(const GameActor* actor) {
	draw_actor(actor, "markers/goal_bar", 0.f, WHITE);
}

static void collide_bar(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || game_state.sequence.type == SEQ_WIN)
		return;

	int32_t points;
	const fixed gy = VAL(actor, GOAL_Y);
	if (actor->pos.y < (gy + FfInt(30L)))
		points = 10000L;
	else if (actor->pos.y < (gy + FfInt(60L)))
		points = 5000L;
	else if (actor->pos.y < (gy + FfInt(100L)))
		points = 2000L;
	else if (actor->pos.y < (gy + FfInt(150L)))
		points = 1000L;
	else if (actor->pos.y < (gy + FfInt(200L)))
		points = 500L;
	else
		points = 200L;
	give_points(actor, get_owner(from), points);
	win_player(from);

	GameActor* bar = create_actor(ACT_GOAL_BAR_FLY, POS_ADD(actor, FfInt(-2L), FfInt(7L)));
	if (bar != NULL) {
		const fixed dir = 154416L + (-12868L + Fmul(FfInt(rng(3L)), 12868L));
		VAL(bar, X_SPEED) = Fmul(327680L, Fcos(dir)), VAL(bar, Y_SPEED) = Fmul(327680L, -Fsin(dir));
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
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, GOAL_ANGLE) += 25736L;
	VAL(actor, Y_SPEED) += 13107L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_fly(const GameActor* actor) {
	draw_actor(actor, "markers/goal_bar2", FtFloat(VAL(actor, GOAL_ANGLE)), WHITE);
}

const GameActorTable TAB_GOAL_BAR_FLY = {.load = load_fly, .create = create_fly, .tick = tick_fly, .draw = draw_fly};
