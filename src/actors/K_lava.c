#include "K_string.h"

#include "actors/K_bowser.h"
#include "actors/K_enemies.h"
#include "actors/K_player.h"

enum {
	VAL_LAVA_Y = VAL_CUSTOM,
	VAL_LAVA_WAVE_LENGTH,
	VAL_LAVA_WAVE_ANGLE,

	VAL_PODOBOO_Y = VAL_CUSTOM,
	VAL_PODOBOO_FIRE = VAL_CUSTOM,
};

enum {
	FLG_LAVA_WAVE = CUSTOM_FLAG(0),

	FLG_PODOBOO_FAST = CUSTOM_FLAG(0),
	FLG_PODOBOO_VOLCANO = CUSTOM_FLAG(1),
	FLG_PODOBOO_START = CUSTOM_FLAG(2),
};

// ====
// LAVA
// ====

static void load() {
	load_texture_num("enemies/lava%u", 8L);
	load_texture("tiles/lava");
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FxFrom(32L);
	actor->depth = FxFrom(20L);

	VAL(actor, LAVA_Y) = actor->pos.y;
}

static void tick(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_LAVA_WAVE))
		return;

	move_actor(actor,
		(FVec2){actor->pos.x,
			VAL(actor, LAVA_Y) + Fmul(VAL(actor, LAVA_WAVE_LENGTH), Fcos(VAL(actor, LAVA_WAVE_ANGLE)))});
	if (VAL(actor, LAVA_WAVE_LENGTH) <= FxZero) {
		FLAG_OFF(actor, FLG_LAVA_WAVE);
		return;
	}

	VAL(actor, LAVA_WAVE_ANGLE) += 5719L;
	VAL(actor, LAVA_WAVE_LENGTH) = Fmax(VAL(actor, LAVA_WAVE_LENGTH) - 6554L, FxZero);
}

static void draw(const GameActor* actor) {
	const InterpActor* iactor = get_interp(actor);
	if (iactor->pos.y != VAL(actor, LAVA_Y)) {
		batch_reset();
		batch_pos(B_XYZ(Fx2Int(iactor->pos.x), Fx2Int(iactor->pos.y) + 16.f, Fx2Float(actor->depth)));
		batch_rectangle("tiles/lava", B_XY(32.f, 48.f));
	}

	const char* tex = fmt("enemies/lava%u", (int)((float)game_state.time / 9.090909090909091f) % 8L);
	draw_actor(actor, tex, 0.f, B_WHITE);
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
		create_actor(ACT_LAVA_SPLASH, POS_ADD(from, FxZero, FxFrom(15L)));
		FLAG_ON(from, FLG_DESTROY);
		break;
	}

	case ACT_BOWSER_DEAD: {
		if (ANY_FLAG(from, FLG_KUPPA_LAVA_LOVE))
			break;

		from->depth = FxFrom(21L);
		FLAG_ON(from, FLG_KUPPA_LAVA_LOVE);
		play_state_sound("bowser_lava", PLAY_POS, 0L, A_ACTOR(from));

		for (ActorID i = 0L; i < 2L; i++) {
			GameActor* laver = create_actor(ACT_BOWSER_LAVA, from->pos);
			if (i > 0L && laver != NULL)
				FLAG_ON(laver, FLG_X_FLIP);
		}

		break;
	}

	case ACT_BOWSER_LAVA: {
		if (ANY_FLAG(actor, FLG_LAVA_WAVE))
			break;
		VAL(actor, LAVA_WAVE_LENGTH) = VAL(from, KUPPA_LAVER);
		VAL(from, KUPPA_LAVER) -= FxFrom(4L);
		FLAG_ON(actor, FLG_LAVA_WAVE);
		break;
	}
	}
}

const GameActorTable TAB_LAVA = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};

// =======
// PODOBOO
// =======

static void load_podoboo() {
	load_texture_num("enemies/podoboo%u", 3L);

	load_sound("kick");

	load_actor(ACT_LAVA_SPLASH);
	load_actor(ACT_POINTS);
}

static void create_podoboo(GameActor* actor) {
	actor->box.start.x = FxFrom(-13L);
	actor->box.start.y = FxFrom(-16L);
	actor->box.end.x = FxFrom(13L);
	actor->box.end.y = FxFrom(16L);
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
			    && in_any_view(actor, FxZero, FALSE))
			|| (!ANY_FLAG(actor, FLG_PODOBOO_FAST) && (game_state.time % 250L) == 0L
				&& in_any_view(actor, FxFrom(608L), FALSE)))
		{
			VAL(actor, Y_SPEED) = FxFrom(-12L);
			FLAG_ON(actor, FLG_VISIBLE);
		} else
			return;
	} else if (!ANY_FLAG(actor, FLG_PODOBOO_VOLCANO) && (actor->pos.y + FxFrom(17L)) >= VAL(actor, PODOBOO_Y)) {
		create_actor(ACT_LAVA_SPLASH, POS_ADD(actor, FxZero, FxFrom(15L)));
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
	const char* tex = fmt("enemies/podoboo%u", (int)((float)game_state.time / 1.666666666666667f) % 3L);
	draw_actor(actor, tex, 0.f, B_WHITE);
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
	actor->box.end.x = actor->box.end.y = FxFrom(32L);
}

static void tick_volcano(GameActor* actor) {
	if ((game_state.time % 300L) == 0L && VAL(actor, PODOBOO_FIRE) == 0L)
		VAL(actor, PODOBOO_FIRE) = 8L;
	if ((game_state.time % 15L) == 0L && VAL(actor, PODOBOO_FIRE) > 0L && in_any_view(actor, FxFrom(224L), FALSE)) {
		GameActor* podoboo = create_actor(ACT_PODOBOO, POS_ADD(actor, FxFrom(16L), FxFrom(10L)));
		if (podoboo != NULL) {
			VAL(podoboo, X_SPEED) = FxFrom(-4L + rng(9L));
			VAL(podoboo, Y_SPEED) = FxFrom(-11L);
			FLAG_ON(podoboo, FLG_PODOBOO_VOLCANO);
			play_state_sound("fire", PLAY_POS, 0L, A_ACTOR(podoboo));
		}
		--VAL(actor, PODOBOO_FIRE);
	}
}

const GameActorTable TAB_PODOBOO_VOLCANO = {.load = load_volcano, .create = create_volcano, .tick = tick_volcano};
