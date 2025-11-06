#include "actors/K_enemies.h"
#include "actors/K_player.h"

enum {
	VAL_PODOBOO_Y = VAL_CUSTOM,

	VAL_PODOBOO_FIRE = VAL_CUSTOM,
};

enum {
	FLG_PODOBOO_FAST = CUSTOM_FLAG(0),
	FLG_PODOBOO_VOLCANO = CUSTOM_FLAG(1),
	FLG_PODOBOO_START = CUSTOM_FLAG(2),
};

// ====
// LAVA
// ====

static void load() {
	load_texture("enemies/lava");
	load_texture("enemies/lava2");
	load_texture("enemies/lava3");
	load_texture("enemies/lava4");
	load_texture("enemies/lava5");
	load_texture("enemies/lava6");
	load_texture("enemies/lava7");
	load_texture("enemies/lava8");
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void draw(const GameActor* actor) {
	const char* tex;
	switch ((int)((float)game_state.time / 9.090909090909091f) % 8L) {
	default:
		tex = "enemies/lava";
		break;
	case 1L:
		tex = "enemies/lava2";
		break;
	case 2L:
		tex = "enemies/lava3";
		break;
	case 3L:
		tex = "enemies/lava4";
		break;
	case 4L:
		tex = "enemies/lava5";
		break;
	case 5L:
		tex = "enemies/lava6";
		break;
	case 6L:
		tex = "enemies/lava7";
		break;
	case 7L:
		tex = "enemies/lava8";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;
	case ACT_PLAYER:
		kill_player(from);
		break;

	case ACT_PODOBOO: {
		if (!ANY_FLAG(from, FLG_PODOBOO_VOLCANO) || VAL(from, Y_SPEED) <= FxZero)
			break;
		create_actor(ACT_LAVA_SPLASH, POS_ADD(from, FxZero, FfInt(15L)));
		FLAG_ON(from, FLG_DESTROY);
		break;
	}
	}
}

const GameActorTable TAB_LAVA = {.load = load, .create = create, .draw = draw, .collide = collide};

// =======
// PODOBOO
// =======

static void load_podoboo() {
	load_texture("enemies/podoboo");
	load_texture("enemies/podoboo2");
	load_texture("enemies/podoboo3");

	load_sound("kick");

	load_actor(ACT_LAVA_SPLASH);
	load_actor(ACT_POINTS);
}

static void create_podoboo(GameActor* actor) {
	actor->box.start.x = FfInt(-13L);
	actor->box.start.y = FfInt(-16L);
	actor->box.end.x = FfInt(13L);
	actor->box.end.y = FfInt(16L);
}

static void tick_podoboo(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PODOBOO_START | FLG_PODOBOO_VOLCANO)) {
		VAL(actor, PODOBOO_Y) = actor->pos.y;
		FLAG_OFF(actor, FLG_VISIBLE);
		FLAG_ON(actor, FLG_PODOBOO_START);
	}

	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	} else if (!ANY_FLAG(actor, FLG_VISIBLE)) {
		if ((ANY_FLAG(actor, FLG_PODOBOO_FAST) && (game_state.time % 65L) == 0L
			    && in_any_view(actor, FxZero, false))
			|| (!ANY_FLAG(actor, FLG_PODOBOO_FAST) && (game_state.time % 250L) == 0L
				&& in_any_view(actor, FfInt(608L), false)))
		{
			VAL(actor, Y_SPEED) = FfInt(-12L);
			FLAG_ON(actor, FLG_VISIBLE);
		} else
			return;
	} else if (!ANY_FLAG(actor, FLG_PODOBOO_VOLCANO) && (actor->pos.y + FfInt(17L)) >= VAL(actor, PODOBOO_Y)) {
		create_actor(ACT_LAVA_SPLASH, POS_ADD(actor, FxZero, FfInt(15L)));
		FLAG_OFF(actor, FLG_VISIBLE);
		return;
	} else
		collide_actor(actor);

	VAL(actor, Y_SPEED) += 13107L;
	if (VAL(actor, Y_SPEED) >= FxZero)
		FLAG_ON(actor, FLG_Y_FLIP);
	move_actor(actor, POS_SPEED(actor));
}

static void draw_podoboo(const GameActor* actor) {
	const char* tex;
	switch ((int)((float)game_state.time / 1.666666666666667f) % 3L) {
	default:
		tex = "enemies/podoboo";
		break;
	case 1L:
		tex = "enemies/podoboo2";
		break;
	case 2L:
		tex = "enemies/podoboo3";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide_podoboo(GameActor* actor, GameActor* from) {
	if (!ANY_FLAG(actor, FLG_VISIBLE))
		return;
	switch (from->type) {
	default:
		break;
	case ACT_PLAYER:
		maybe_hit_player(actor, from);
		break;
	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 500L);
		break;
	}
}

const GameActorTable TAB_PODOBOO = {
	.load = load_podoboo,
	.create = create_podoboo,
	.tick = tick_podoboo,
	.draw = draw_podoboo,
	.collide = collide_podoboo,
};

// ===============
// PODOBOO VOLCANO
// ===============

static void load_volcano(GameActor* actor) {
	load_sound("fire");

	load_actor(ACT_PODOBOO);
}

static void create_volcano(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick_volcano(GameActor* actor) {
	if ((game_state.time % 300L) == 0L && VAL(actor, PODOBOO_FIRE) == 0L)
		VAL(actor, PODOBOO_FIRE) = 8L;
	if ((game_state.time % 15L) == 0L && VAL(actor, PODOBOO_FIRE) > 0L && in_any_view(actor, FfInt(224L), false)) {
		GameActor* podoboo = create_actor(ACT_PODOBOO, POS_ADD(actor, FfInt(16L), FfInt(10L)));
		if (podoboo != NULL) {
			VAL(podoboo, X_SPEED) = FfInt(-4L + rng(9L));
			VAL(podoboo, Y_SPEED) = FfInt(-11L);
			FLAG_ON(podoboo, FLG_PODOBOO_VOLCANO);
			play_actor_sound(podoboo, "fire");
		}
		--VAL(actor, PODOBOO_FIRE);
	}
}

const GameActorTable TAB_PODOBOO_VOLCANO = {.load = load_volcano, .create = create_volcano, .tick = tick_volcano};
