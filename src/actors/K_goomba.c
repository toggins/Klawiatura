#include "K_string.h"

#include "actors/K_enemies.h"

enum {
	VAL_GOOMBA_FLAT = VAL_CUSTOM
};

// ======
// GOOMBA
// ======

static void load() {
	load_texture_num("enemies/goomba%u", 2L, FALSE);

	load_sound("stomp", FALSE);
	load_sound("kick", FALSE);

	load_actor(ACT_GOOMBA_FLAT);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FxFrom(-15L);
	actor->box.start.y = FxFrom(-32L);
	actor->box.end.x = FxFrom(15L);

	increase_ambush();
}

static void tick(GameActor* actor) {
	move_enemy(actor, (FVec2){FxOne, 19005L}, FALSE);
}

static void draw(const GameActor* actor) {
	draw_actor(
		actor, fmt("enemies/goomba%u", (int)((float)game_state.time / 9.090909090909091f) % 2L), 0.f, B_WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/goomba0", 0.f, B_WHITE);
}

static void cleanup(GameActor* actor) {
	decrease_ambush();
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, FxFrom(-16L), 100L)) {
			GameActor* flat = create_actor(ACT_GOOMBA_FLAT, actor->pos);
			if (flat != NULL) {
				VAL(flat, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
				align_interp(flat, actor);
			}
			mark_ambush_winner(from);
			FLAG_ON(actor, FLG_DESTROY);
		} else
			maybe_hit_player(actor, from);
		break;
	}

	case ACT_GOOMBA:
	case ACT_KOOPA:
	case ACT_SPINY:
	case ACT_SPINY_EGG:
	case ACT_BUZZY_BEETLE:
	case ACT_CHEEP_CHEEP_BLUE: {
		turn_enemy(actor);
		turn_enemy(from);
		break;
	}

	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL: {
		if (!hit_shell(actor, from))
			turn_enemy(actor);
		break;
	}

	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 100L);
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

const GameActorTable TAB_GOOMBA = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_dead = draw_corpse,
	.cleanup = cleanup,
	.collide = collide,
};

// ===========
// FLAT GOOMBA
// ===========

static void load_flat() {
	load_texture("enemies/goomba_flat", FALSE);
}

static void create_flat(GameActor* actor) {
	actor->box.start.x = FxFrom(-15L);
	actor->box.start.y = FxFrom(-16L);
	actor->box.end.x = FxFrom(15L);
}

static void tick_flat(GameActor* actor) {
	if (++VAL(actor, GOOMBA_FLAT) > 200L || below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, Y_SPEED) += 19005L;
	displace_actor(actor, FxZero, FALSE);
}

static void draw_flat(const GameActor* actor) {
	draw_actor(actor, "enemies/goomba_flat", 0.f, B_WHITE);
}

const GameActorTable TAB_GOOMBA_FLAT = {.load = load_flat, .create = create_flat, .tick = tick_flat, .draw = draw_flat};
