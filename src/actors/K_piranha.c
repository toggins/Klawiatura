#include "K_game.h"
#include "K_string.h"

#include "actors/K_enemies.h"
#include "actors/K_missiles.h" // IWYU pragma: keep
#include "actors/K_player.h"

enum {
	VAL_PIRANHA_MOVE = VAL_CUSTOM,
	VAL_PIRANHA_WAIT,
	VAL_PIRANHA_FIRE,
};

enum {
	FLG_PIRANHA_FIRE = CUSTOM_FLAG(0),
	FLG_PIRANHA_RED = CUSTOM_FLAG(1),
	FLG_PIRANHA_START = CUSTOM_FLAG(2),
	FLG_PIRANHA_BLOCKED = CUSTOM_FLAG(3),
	FLG_PIRANHA_OUT = CUSTOM_FLAG(4),
	FLG_PIRANHA_IN = CUSTOM_FLAG(5),
	FLG_PIRANHA_FIRED = CUSTOM_FLAG(6),
};

// =============
// PIRANHA PLANT
// =============

static void load() {
	load_texture_num("enemies/piranha%u", 2L);
	load_texture_num("enemies/piranha_red%u", 2L);
	load_texture_num("enemies/piranha_fire%u", 2L);

	load_sound("fire");
	load_sound("kick");

	load_actor(ACT_PIRANHA_FIRE);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-45L);
	actor->box.end.x = FfInt(15L);
	actor->box.end.y = FxOne;

	FLAG_ON(actor, FLG_PIRANHA_FIRED);
}

static void tick(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PIRANHA_START)) {
		if (ANY_FLAG(actor, FLG_Y_FLIP)) {
			actor->box.start.y = -FxOne;
			actor->box.end.y = FfInt(45L);
			move_actor(actor, POS_ADD(actor, FxZero, FfInt(-60L)));
		} else
			move_actor(actor, POS_ADD(actor, FxZero, FfInt(60L)));
		skip_interp(actor);
		FLAG_ON(actor, FLG_PIRANHA_START);
	}

	if ((game_state.time % 10L) == 0L)
		FLAG_OFF(actor, FLG_PIRANHA_BLOCKED);

	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		const fixed range = ANY_FLAG(actor, FLG_PIRANHA_RED) ? FfInt(40L) : FfInt(80L);
		if (actor->pos.x < (nearest->pos.x + range) && actor->pos.x > (nearest->pos.x - range))
			FLAG_ON(actor, FLG_PIRANHA_BLOCKED);
	}

	if (!ANY_FLAG(actor, FLG_PIRANHA_BLOCKED | FLG_PIRANHA_OUT | FLG_PIRANHA_IN)
		&& in_any_view(actor, FfInt(96L), false))
	{
		FLAG_ON(actor, FLG_PIRANHA_OUT);
		VAL(actor, PIRANHA_MOVE) = -60L;
	}

	if (VAL(actor, PIRANHA_MOVE) < 0L && ANY_FLAG(actor, FLG_PIRANHA_OUT)) {
		const ActorValue move = ANY_FLAG(actor, FLG_PIRANHA_RED) ? 2L : 1L;
		move_actor(actor, POS_ADD(actor, FxZero, FfInt(ANY_FLAG(actor, FLG_Y_FLIP) ? move : -move)));
		VAL(actor, PIRANHA_MOVE) += move;
	}

	if (VAL(actor, PIRANHA_MOVE) == 0L && ANY_FLAG(actor, FLG_PIRANHA_OUT) && !ANY_FLAG(actor, FLG_PIRANHA_IN))
		++VAL(actor, PIRANHA_WAIT);

	if (ANY_FLAG(actor, FLG_PIRANHA_OUT) && !ANY_FLAG(actor, FLG_PIRANHA_IN) && VAL(actor, PIRANHA_WAIT) > 50L) {
		VAL(actor, PIRANHA_WAIT) = 0L, VAL(actor, PIRANHA_MOVE) = 60L;
		FLAG_ON(actor, FLG_PIRANHA_IN | FLG_PIRANHA_FIRED);
	}

	if (VAL(actor, PIRANHA_MOVE) == 0L && ALL_FLAG(actor, FLG_PIRANHA_OUT | FLG_PIRANHA_FIRE | FLG_PIRANHA_FIRED)
		&& !ANY_FLAG(actor, FLG_PIRANHA_IN))
	{
		VAL(actor, PIRANHA_FIRE) = (game_state.flags & GF_FUNNY) ? 30L : 3L;
		FLAG_OFF(actor, FLG_PIRANHA_FIRED);
	}

	if (VAL(actor, PIRANHA_MOVE) > 0L && ANY_FLAG(actor, FLG_PIRANHA_OUT)) {
		const ActorValue move = ANY_FLAG(actor, FLG_PIRANHA_RED) ? 2L : 1L;
		move_actor(actor, POS_ADD(actor, FxZero, FfInt(ANY_FLAG(actor, FLG_Y_FLIP) ? -move : move)));
		VAL(actor, PIRANHA_MOVE) -= move;
	}

	if (VAL(actor, PIRANHA_MOVE) == 0L && ALL_FLAG(actor, FLG_PIRANHA_OUT | FLG_PIRANHA_IN))
		++VAL(actor, PIRANHA_WAIT);

	if (ALL_FLAG(actor, FLG_PIRANHA_OUT | FLG_PIRANHA_IN) && VAL(actor, PIRANHA_WAIT) > 50L) {
		VAL(actor, PIRANHA_WAIT) = 0L;
		FLAG_OFF(actor, FLG_PIRANHA_OUT | FLG_PIRANHA_IN);
	}

	if ((game_state.time % ((game_state.flags & GF_FUNNY) ? 2L : 10L)) == 0L && VAL(actor, PIRANHA_FIRE) > 0L) {
		const Bool flip = ANY_FLAG(actor, FLG_Y_FLIP);

		GameActor* fire
			= create_actor(ACT_PIRANHA_FIRE, POS_ADD(actor, FxZero, flip ? FfInt(35L) : FfInt(-35L)));
		if (fire != NULL) {
			VAL(fire, X_SPEED) = FfInt(-4L + rng(9L));
			VAL(fire, Y_SPEED) = FfInt(flip ? rng(6L) : (-3L - rng(9L)));
			play_actor_sound(fire, "fire");
		}

		--VAL(actor, PIRANHA_FIRE);
	}
}

static void draw(const GameActor* actor) {
	const char* tex = NULL;
	if (ANY_FLAG(actor, FLG_PIRANHA_FIRE))
		tex = fmt("enemies/piranha_fire%u", (int)((float)game_state.time / 4.166666666666667f) % 2L);
	else if (ANY_FLAG(actor, FLG_PIRANHA_RED))
		tex = fmt("enemies/piranha_red%u", (game_state.time / 7L) % 2L);
	else
		tex = fmt("enemies/piranha%u", (int)((float)game_state.time / 7.142857142857143f) % 2L);

	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;
	case ACT_PLAYER:
		maybe_hit_player(actor, from);
		break;
	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 100L);
		break;
	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
	case ACT_MISSILE_FIREBALL:
		hit_fireball(actor, from, 100L);
		break;
	case ACT_MISSILE_BEETROOT:
		hit_beetroot(actor, from, 100L);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 100L);
		break;
	}
}

const GameActorTable TAB_PIRANHA_PLANT = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.collide = collide,
};

// ============
// PIRANHA FIRE
// ============

static void load_fire() {
	load_texture("missiles/fireball");
}

static void create_fire(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-7L);
	actor->box.end.x = actor->box.end.y = FfInt(8L);
}

static void tick_fire(GameActor* actor) {
	if (!in_any_view(actor, FxZero, true)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, MISSILE_ANGLE) += 12868L;
	VAL(actor, Y_SPEED) += 13107L;
	move_actor(actor, POS_SPEED(actor));
}

static void draw_fire(const GameActor* actor) {
	draw_actor(actor, "missiles/fireball", FtFloat(VAL(actor, MISSILE_ANGLE)), B_WHITE);
}

static void collide_fire(GameActor* actor, GameActor* from) {
	hit_player(from);
}

const GameActorTable TAB_PIRANHA_FIRE = {
	.load = load_fire,
	.create = create_fire,
	.tick = tick_fire,
	.draw = draw_fire,
	.collide = collide_fire,
};

// ============
// PIRANHA HEAD
// ============

static void load_head() {
	load_texture_num("enemies/piranha_head%u", 3L);
	load_texture_num("enemies/piranha_head_fire%u", 3L);

	load_sound("fire");

	load_actor(ACT_PIRANHA_FIRE);
}

static void create_head(GameActor* actor) {
	actor->box.start.y = FxOne;
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(32L);

	actor->depth = FxOne;
}

static void tick_head(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PIRANHA_START)) {
		if (ANY_FLAG(actor, FLG_Y_FLIP)) {
			actor->box.start.y = FfInt(-32L);
			actor->box.end.y = -FxOne;
		}
		FLAG_ON(actor, FLG_PIRANHA_START);
	}

	if (!ANY_FLAG(actor, FLG_PIRANHA_FIRE))
		return;

	if (VAL(actor, PIRANHA_FIRE) > 0L && (game_state.time % ((game_state.flags & GF_FUNNY) ? 2L : 10L)) == 0L) {
		const Bool flip = ANY_FLAG(actor, FLG_Y_FLIP);

		GameActor* fire
			= create_actor(ACT_PIRANHA_FIRE, POS_ADD(actor, FfInt(15L), flip ? FfInt(-16L) : FfInt(16L)));
		if (fire != NULL) {
			VAL(fire, X_SPEED) = FfInt(-4L + rng(9L));
			VAL(fire, Y_SPEED) = FfInt(flip ? rng(6L) : (-3L - rng(9L)));
			play_actor_sound(fire, "fire");
		}

		--VAL(actor, PIRANHA_FIRE);
	}

	if (in_any_view(actor, FxZero, false))
		if (++VAL(actor, PIRANHA_WAIT) >= 200L) {
			VAL(actor, PIRANHA_FIRE) = (game_state.flags & GF_FUNNY) ? 30L : 3L;
			VAL(actor, PIRANHA_WAIT) = 0L;
		}
}

static void draw_head(const GameActor* actor) {
	const char* tex = NULL;
	if (ANY_FLAG(actor, FLG_PIRANHA_FIRE))
		switch ((int)((float)game_state.time / 2.7027027027f) % 4L) {
		default:
			tex = "enemies/piranha_head_fire0";
			break;
		case 1L:
		case 3L:
			tex = "enemies/piranha_head_fire1";
			break;
		case 2L:
			tex = "enemies/piranha_head_fire2";
			break;
		}
	else
		switch ((int)((float)game_state.time / 4.54545454545f) % 4L) {
		default:
			tex = "enemies/piranha_head0";
			break;
		case 1L:
		case 3L:
			tex = "enemies/piranha_head1";
			break;
		case 2L:
			tex = "enemies/piranha_head2";
			break;
		}

	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide_head(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER)
		hit_player(from);
}

const GameActorTable TAB_PIRANHA_HEAD = {
	.load = load_head,
	.create = create_head,
	.tick = tick_head,
	.draw = draw_head,
	.collide = collide_head,
};
