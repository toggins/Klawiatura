#include "actors/K_enemies.h"
#include "actors/K_player.h"

// ================
// BLUE CHEEP CHEEP
// ================

static void load_blue() {
	load_texture("enemies/cheep_blue");
	load_texture("enemies/cheep_blue2");

	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void create_blue(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(15L);
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
	.create = create_blue,
	.tick = tick_blue,
	.draw = draw_blue,
	.draw_dead = draw_blue_corpse,
	.collide = collide_blue,
};

// =================
// SPIKY CHEEP CHEEP
// =================
