#include "actors/K_enemies.h"
#include "actors/K_player.h"

// ===========
// CHEEP CHEEP
// ===========

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(15L);
}

// ================
// BLUE CHEEP CHEEP
// ================

static void load_blue() {
	load_texture_wild("enemies/cheep_blue?");

	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void tick_blue(GameActor* actor) {
	move_enemy(actor, (fvec2){FxOne, FxZero}, false);
}

static void draw_blue(const GameActor* actor) {
	draw_actor(actor,
		((int)((float)game_state.time / 11.11111111111111f) % 2L) ? "enemies/cheep_blue2"
									  : "enemies/cheep_blue",
		0.f, WHITE);
}

static void draw_blue_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/cheep_blue", 0.f, WHITE);
}

static void collide_blue(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (!ANY_FLAG(from, FLG_PLAYER_SWIM) && check_stomp(actor, from, FfInt(-16L), 200L))
			kill_enemy(actor, from, false);
		else
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
		hit_bump(actor, from, 200L);
		break;
	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	case ACT_MISSILE_BEETROOT:
		block_beetroot(from);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 200L);
		break;
	}
}

const GameActorTable TAB_CHEEP_CHEEP_BLUE = {
	.load = load_blue,
	.create = create,
	.tick = tick_blue,
	.draw = draw_blue,
	.draw_dead = draw_blue_corpse,
	.collide = collide_blue,
};

// =================
// SPIKY CHEEP CHEEP
// =================

static void load_spiky() {
	load_texture_wild("enemies/cheep_spiky?");

	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void tick_spiky(GameActor* actor) {
	fix16_t dir = FxZero, spd = FxZero;

	if (!ANY_FLAG(actor, FLG_ENEMY_ACTIVE)) {
		spd = -FxOne;
		if (in_any_view(actor, FxZero, false))
			FLAG_ON(actor, FLG_ENEMY_ACTIVE);
	} else if (in_any_view(actor, FfInt(224L), false)) {
		if ((game_state.time % 50L) == 0L) {
			GameActor* nearest = nearest_pawn(actor->pos);
			if (nearest != NULL) {
				dir = Vtheta(actor->pos, POS_ADD(nearest, FxZero, FfInt(-14L)));
				spd = 81920L;
			}
		}
	} else
		FLAG_OFF(actor, FLG_ENEMY_ACTIVE);

	if ((actor->pos.y - FxOne) < game_state.water) {
		dir = 167284L - Fmul(FfInt(rng(7L)), 12868L);
		spd = 81920L;
	}

	if (spd < FxZero) {
		VAL(actor, X_SPEED) = VAL(actor, Y_SPEED) = FxZero;
	} else if (spd > FxZero) {
		VAL(actor, X_SPEED) = Fmul(Fcos(dir), spd);
		VAL(actor, Y_SPEED) = Fmul(Fsin(dir), spd);
		if (VAL(actor, X_SPEED) < FxZero)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (VAL(actor, X_SPEED) > FxZero)
			FLAG_OFF(actor, FLG_X_FLIP);
	}

	move_actor(actor, POS_SPEED(actor));
}

static void draw_spiky(const GameActor* actor) {
	draw_actor(actor, ((int)((float)game_state.time / 6.25f) % 2L) ? "enemies/cheep_spiky2" : "enemies/cheep_spiky",
		0.f, WHITE);
}

static void draw_spiky_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/cheep_blue", 0.f, WHITE);
}

static void collide_spiky(GameActor* actor, GameActor* from) {
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
	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 500L);
		break;
	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	case ACT_MISSILE_BEETROOT:
		block_beetroot(from);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 500L);
		break;
	}
}

const GameActorTable TAB_CHEEP_CHEEP_SPIKY = {
	.load = load_spiky,
	.create = create,
	.tick = tick_spiky,
	.draw = draw_spiky,
	.draw_dead = draw_spiky_corpse,
	.collide = collide_spiky,
};
