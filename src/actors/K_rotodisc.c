#include "K_string.h"

#include "actors/K_enemies.h"
#include "actors/K_rotodisc.h"

// ==============
// ROTO-DISC BALL
// ==============

static void load_ball() {
	load_texture("enemies/rotodisc_ball", FALSE);

	load_actor(ACT_ROTODISC);
}

static void create_ball(GameActor* actor) {
	actor->depth = FfInt(19L);

	VAL(actor, ROTODISC_CHILD) = NULLACT;
}

static void tick_ball(GameActor* actor) {
	if (ANY_FLAG(actor, FLG_ROTODISC_START))
		return;

	GameActor* rotodisc = create_actor(ACT_ROTODISC, actor->pos);
	if (rotodisc != NULL) {
		VAL(rotodisc, ROTODISC_PARENT) = actor->id;
		VAL(rotodisc, ROTODISC_LENGTH) = VAL(actor, ROTODISC_LENGTH);
		VAL(rotodisc, ROTODISC_ANGLE) = VAL(actor, ROTODISC_ANGLE);
		VAL(rotodisc, ROTODISC_SPEED) = VAL(actor, ROTODISC_SPEED);
		FLAG_ON(rotodisc, actor->flags & FLG_ROTODISC_FLOWER);

		VAL(actor, ROTODISC_CHILD) = rotodisc->id;
	}

	FLAG_ON(actor, FLG_ROTODISC_START);
}

static void draw_ball(const GameActor* actor) {
	draw_actor(actor, "enemies/rotodisc_ball", 0.f, B_WHITE);
}

static void cleanup_ball(GameActor* actor) {
	GameActor* rotodisc = get_actor(VAL(actor, ROTODISC_CHILD));
	if (rotodisc != NULL)
		FLAG_ON(rotodisc, FLG_DESTROY);
}

const GameActorTable TAB_ROTODISC_BALL
	= {.load = load_ball, .create = create_ball, .tick = tick_ball, .draw = draw_ball, .cleanup = cleanup_ball};

// =========
// ROTO-DISC
// =========

static void load() {
	load_texture_num("enemies/rotodisc%u", 26L, FALSE);
}

static void create(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FxOne;
	actor->box.end.x = actor->box.end.y = FfInt(30L);

	VAL(actor, ROTODISC_PARENT) = NULLACT;
}

static void tick(GameActor* actor) {
	GameActor* owner = get_actor(VAL(actor, ROTODISC_PARENT));
	if (owner == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (ANY_FLAG(actor, FLG_ROTODISC_FLOWER)) {
		VAL(actor, ROTODISC_LENGTH) += ANY_FLAG(actor, FLG_ROTODISC_FLOWER2) ? FfInt(-5L) : FfInt(5L);
		if (VAL(actor, ROTODISC_LENGTH) >= FfInt(200L) && !ANY_FLAG(actor, FLG_ROTODISC_FLOWER2))
			FLAG_ON(actor, FLG_ROTODISC_FLOWER2);
		else if (VAL(actor, ROTODISC_LENGTH) <= FxZero && ANY_FLAG(actor, FLG_ROTODISC_FLOWER2))
			FLAG_OFF(actor, FLG_ROTODISC_FLOWER2);
	}

	VAL(actor, ROTODISC_ANGLE) += VAL(actor, ROTODISC_SPEED);
	move_actor(actor,
		(FVec2){Fadd(owner->pos.x, Fmul(Fcos(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH))),
			Fadd(owner->pos.y, Fmul(Fsin(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH)))});
}

static void draw(const GameActor* actor) {
	draw_actor(actor, fmt("enemies/rotodisc%u", game_state.time % 26L), 0.f, B_WHITE);
}

static void cleanup(GameActor* actor) {
	GameActor* owner = get_actor(VAL(actor, ROTODISC_PARENT));
	if (owner != NULL && owner->type == ACT_ROTODISC_BALL)
		VAL(owner, ROTODISC_CHILD) = NULLACT;
}

static void collide(GameActor* actor, GameActor* from) {
	maybe_hit_player(actor, from);
}

const GameActorTable TAB_ROTODISC
	= {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup, .collide = collide};
