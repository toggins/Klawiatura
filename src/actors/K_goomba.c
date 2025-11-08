#include "actors/K_enemies.h"

enum {
	VAL_GOOMBA_FLAT = VAL_CUSTOM
};

// ======
// GOOMBA
// ======

static void load() {
	load_texture("enemies/goomba");
	load_texture("enemies/goomba2");

	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_GOOMBA_FLAT);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(15L);

	increase_ambush();
}

static void tick(GameActor* actor) {
	move_enemy(actor, (fvec2){FxOne, 19005L}, false);
}

static void draw(const GameActor* actor) {
	draw_actor(actor,
		((int)((float)game_state.time / 9.090909090909091f) % 2L) ? "enemies/goomba2" : "enemies/goomba", 0.f,
		WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/goomba", 0.f, WHITE);
}

static void cleanup(GameActor* actor) {
	decrease_ambush();
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, FfInt(-16L), 100L)) {
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
	load_texture("enemies/goomba_flat");
}

static void create_flat(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-16L);
	actor->box.end.x = FfInt(15L);
}

static void tick_flat(GameActor* actor) {
	if (++VAL(actor, GOOMBA_FLAT) > 200L || below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, Y_SPEED) += 19005L;
	displace_actor(actor, FxZero, false);
}

static void draw_flat(const GameActor* actor) {
	draw_actor(actor, "enemies/goomba_flat", 0.f, WHITE);
}

const GameActorTable TAB_GOOMBA_FLAT = {.load = load_flat, .create = create_flat, .tick = tick_flat, .draw = draw_flat};
