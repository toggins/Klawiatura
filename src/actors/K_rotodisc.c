#include "actors/K_enemies.h"
#include "actors/K_rotodisc.h"

// ==============
// ROTO-DISC BALL
// ==============

static void load_ball() {
	load_texture("enemies/rotodisc_ball");

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
	draw_actor(actor, "enemies/rotodisc_ball", 0.f, WHITE);
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
	load_texture_wild("enemies/rotodisc??");
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
		(fvec2){Fadd(owner->pos.x, Fmul(Fcos(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH))),
			Fadd(owner->pos.y, Fmul(Fsin(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH)))});
}

static void draw(const GameActor* actor) {
	const char* tex = NULL;
	switch (game_state.time % 26L) {
	default:
		tex = "enemies/rotodisc";
		break;
	case 1L:
		tex = "enemies/rotodisc2";
		break;
	case 2L:
		tex = "enemies/rotodisc3";
		break;
	case 3L:
		tex = "enemies/rotodisc4";
		break;
	case 4L:
		tex = "enemies/rotodisc5";
		break;
	case 5L:
		tex = "enemies/rotodisc6";
		break;
	case 6L:
		tex = "enemies/rotodisc7";
		break;
	case 7L:
		tex = "enemies/rotodisc8";
		break;
	case 8L:
		tex = "enemies/rotodisc9";
		break;
	case 9L:
		tex = "enemies/rotodisc10";
		break;
	case 10L:
		tex = "enemies/rotodisc11";
		break;
	case 11L:
		tex = "enemies/rotodisc12";
		break;
	case 12L:
		tex = "enemies/rotodisc13";
		break;
	case 13L:
		tex = "enemies/rotodisc14";
		break;
	case 14L:
		tex = "enemies/rotodisc15";
		break;
	case 15L:
		tex = "enemies/rotodisc16";
		break;
	case 16L:
		tex = "enemies/rotodisc17";
		break;
	case 17L:
		tex = "enemies/rotodisc18";
		break;
	case 18L:
		tex = "enemies/rotodisc19";
		break;
	case 19L:
		tex = "enemies/rotodisc20";
		break;
	case 20L:
		tex = "enemies/rotodisc21";
		break;
	case 21L:
		tex = "enemies/rotodisc22";
		break;
	case 22L:
		tex = "enemies/rotodisc23";
		break;
	case 23L:
		tex = "enemies/rotodisc24";
		break;
	case 24L:
		tex = "enemies/rotodisc25";
		break;
	case 25L:
		tex = "enemies/rotodisc26";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
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
