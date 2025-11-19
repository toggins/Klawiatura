#include "K_string.h"

#include "actors/K_spiny.h"

// =====
// SPINY
// =====

static void load() {
	load_texture_num("enemies/spiny%u", 2L);
	load_texture_num("enemies/spiny_gray%u", 2L);

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
	move_enemy(actor,
		(fvec2){((game_state.flags & GF_HARDCORE) && !ANY_FLAG(actor, FLG_SPINY_GRAY)) ? FfInt(2L) : FxOne,
			19005L},
		false);
}

static void draw(const GameActor* actor) {
	const char* tex = fmt(ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny_gray%u" : "enemies/spiny%u",
		(int)((float)game_state.time / 7.142857142857143f) % 2L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny_gray1" : "enemies/spiny1", 0.f, B_WHITE);
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

	case ACT_MISSILE_FIREBALL: {
		if (ANY_FLAG(actor, FLG_SPINY_GRAY))
			block_fireball(from);
		else
			hit_fireball(actor, from, 100L);
		break;
	}

	case ACT_MISSILE_BEETROOT: {
		if (ANY_FLAG(actor, FLG_SPINY_GRAY))
			block_beetroot(from);
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

// =========
// SPINY EGG
// =========

static void load_egg() {
	load_texture_num("enemies/spiny_egg%u", 6L);
	load_texture("enemies/spiny_egg_green");

	load_sound("kick");

	load_actor(ACT_SPINY);
	load_actor(ACT_POINTS);
}

static void create_egg(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-15L);
	actor->box.end.x = actor->box.end.y = FfInt(15L);
}

static void tick_egg(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (!ANY_FLAG(actor, FLG_SPINY_HATCH) || ANY_FLAG(actor, FLG_SPINY_GREEN))
		VAL(actor, SPINY_ANGLE) += (VAL(actor, X_SPEED) < FxZero) ? -25736L : 25736L;
	if (ALL_FLAG(actor, FLG_SPINY_ROLL)) {
		move_enemy(actor, (fvec2){FfInt(2L), 19005L}, false);
		return;
	}

	if (ANY_FLAG(actor, FLG_SPINY_GREEN))
		VAL(actor, Y_SPEED) += 19005L;
	else if (VAL(actor, Y_SPEED) < FfInt(8L))
		VAL(actor, Y_SPEED) += 8738L;

	const fixed spd = VAL(actor, X_SPEED);
	if (touching_solid(HITBOX(actor), SOL_SOLID))
		move_actor(actor, POS_SPEED(actor));
	else
		displace_actor(actor, FfInt(10L), false);

	if (!ANY_FLAG(actor, FLG_SPINY_GREEN) && ANY_FLAG(actor, FLG_SPINY_HATCH)) {
		if (++VAL(actor, SPINY_FRAME) >= 5L) {
			GameActor* spiny = create_actor(ACT_SPINY, actor->pos);
			if (spiny != NULL) {
				align_interp(spiny, actor);
				GameActor* nearest = nearest_pawn(actor->pos);
				FLAG_ON(spiny, (nearest != NULL && nearest->pos.x < actor->pos.x) * FLG_X_FLIP);
			}

			FLAG_ON(actor, FLG_DESTROY);
		}
	} else {
		if (VAL(actor, X_TOUCH) != 0L)
			VAL(actor, X_SPEED) = VAL(actor, X_TOUCH) * -Fabs(spd);

		if (VAL(actor, Y_TOUCH) > 0L) {
			if (ANY_FLAG(actor, FLG_SPINY_GREEN)) {
				GameActor* nearest = nearest_pawn(actor->pos);

				if (ANY_FLAG(actor, FLG_SPINY_HATCH)) {
					if (nearest != NULL) {
						if (nearest->pos.x < actor->pos.x)
							FLAG_ON(actor, FLG_X_FLIP);
						else
							FLAG_OFF(actor, FLG_X_FLIP);
					}

					FLAG_ON(actor, FLG_SPINY_ROLL);
					return;
				}

				VAL(actor, Y_SPEED) = FfInt(-4L);

				if (nearest != NULL) {
					if (nearest->pos.x < actor->pos.x) {
						VAL(actor, X_SPEED) = FfInt(-3L);
						FLAG_ON(actor, FLG_X_FLIP);
					} else {
						VAL(actor, X_SPEED) = FfInt(3L);
						FLAG_OFF(actor, FLG_X_FLIP);
					}
				}

				FLAG_ON(actor, FLG_SPINY_HATCH);
				return;
			}

			actor->box.start.y = FfInt(-32L);
			actor->box.end.y = FxZero;

			move_actor(actor, POS_ADD(actor, FxZero, FfInt(15L)));
			skip_interp(actor);

			VAL(actor, SPINY_FRAME) = 0L;
			VAL(actor, SPINY_ANGLE) = FxZero;
			FLAG_ON(actor, FLG_SPINY_HATCH);
		}
	}
}

static void draw_egg(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_SPINY_GREEN)) {
		draw_actor(actor, "enemies/spiny_egg_green", FtFloat(VAL(actor, SPINY_ANGLE)), B_WHITE);
		return;
	}

	const char* tex = ANY_FLAG(actor, FLG_SPINY_HATCH) ? fmt("enemies/spiny_egg%u", 1L + VAL(actor, SPINY_FRAME))
	                                                   : "enemies/spiny_egg0";
	draw_actor(actor, tex, FtFloat(VAL(actor, SPINY_ANGLE)), B_WHITE);
}

static void draw_egg_corpse(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_SPINY_GREEN))
		draw_actor_offset(actor, "enemies/spiny_egg_green", B_XY(0.f, 31.f), 0.f, B_WHITE);
	else
		draw_actor(actor, "enemies/spiny1", 0.f, B_WHITE);
}

const GameActorTable TAB_SPINY_EGG = {
	.load = load_egg,
	.create = create_egg,
	.tick = tick_egg,
	.draw = draw_egg,
	.draw_dead = draw_egg_corpse,
	.collide = collide,
};
