#include "actors/K_enemies.h"

enum {
	FLG_SPINY_GRAY = CUSTOM_ENEMY_FLAG(0),
};

static void load() {
	load_texture("enemies/spiny");
	load_texture("enemies/spiny2");
	load_texture("enemies/spiny_gray");
	load_texture("enemies/spiny_gray2");

	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(15L);

	increase_ambush();
}

static void tick(GameActor* actor) {
	move_enemy(actor, ((game_state.flags & GF_HARDCORE) && !ANY_FLAG(actor, FLG_SPINY_GRAY)) ? FfInt(2L) : FxOne,
		false);
}

static void draw(const GameActor* actor) {
	const char* tex;
	if ((int)((float)game_state.time / 7.142857142857143f) % 2L)
		tex = ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny_gray2" : "enemies/spiny2";
	else
		tex = ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny_gray" : "enemies/spiny";
	draw_actor(actor, tex, 0.f, WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny_gray2" : "enemies/spiny2", 0.f, WHITE);
}

static void cleanup(GameActor* actor) {
	decrease_ambush();
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;
	case ACT_PLAYER:
		maybe_hit_player(actor, from);
		break;

	case ACT_GOOMBA:
	case ACT_KOOPA:
	case ACT_SPINY:
	case ACT_BUZZY_BEETLE: {
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

	case ACT_MISSILE_FIREBALL: {
		if (ANY_FLAG(actor, FLG_SPINY_GRAY))
			block_fireball(actor);
		else
			hit_fireball(actor, from, 100L);
		break;
	}

	case ACT_MISSILE_BEETROOT: {
		if (ANY_FLAG(actor, FLG_SPINY_GRAY))
			block_beetroot(actor);
		else
			hit_beetroot(actor, from, 100L);
		break;
	}

	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 100L);
		break;
	}
}

const GameActorTable TAB_SPINY = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_dead = draw_corpse,
	.cleanup = cleanup,
	.collide = collide,
};
