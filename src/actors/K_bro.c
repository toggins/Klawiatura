#include "actors/K_enemies.h"

enum {
	VAL_BRO_MISSILE = VAL_CUSTOM,
	VAL_BRO_MOVE,
	VAL_BRO_TEMP2,
	VAL_BRO_TEMP3,
	VAL_BRO_TEMP4,
	VAL_BRO_TEMP5,
	VAL_BRO_JUMP_STATE,
	VAL_BRO_JUMP,
	VAL_BRO_DOWN,
	VAL_BRO_THROW_STATE,
	VAL_BRO_THROW,
};

enum {
	FLG_BRO_ACTIVE = CUSTOM_FLAG(0),
	FLG_BRO_TEMP10 = CUSTOM_FLAG(1),
	FLG_BRO_TEMP11 = CUSTOM_FLAG(2),
	FLG_BRO_JUMP = CUSTOM_FLAG(3),
	FLG_BRO_TEMP30 = CUSTOM_FLAG(3),
	FLG_BRO_TEMP31 = CUSTOM_FLAG(4),
	FLG_BRO_BOTTOM = CUSTOM_FLAG(5),
	FLG_BRO_TOP = CUSTOM_FLAG(6),

	FLG_BRO_LAYER_TOP = CUSTOM_FLAG(0),
};

// ===
// BRO
// ===

static void load() {
	load_texture("enemies/bro");
	load_texture("enemies/bro2");
	load_texture("enemies/bro_hammer");
	load_texture("enemies/bro_hammer2");
	load_texture("enemies/bro_fire");
	load_texture("enemies/bro_fire2");
	load_texture("enemies/bro_silver");
	load_texture("enemies/bro_silver2");

	load_sound("hammer");
	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_MISSILE_HAMMER);
	load_actor(ACT_MISSILE_FIREBALL);
	load_actor(ACT_MISSILE_SILVER_HAMMER);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-48L);
	actor->box.end.x = FfInt(15L);

	increase_ambush();
}

static void tick(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		if (nearest->pos.x < actor->pos.x)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (nearest->pos.x > actor->pos.x)
			FLAG_OFF(actor, FLG_X_FLIP);
	}

	if (ANY_FLAG(actor, FLG_BRO_ACTIVE)) {
		if (VAL(actor, BRO_MOVE) == 0L)
			VAL(actor, BRO_TEMP2) += VAL(actor, BRO_TEMP5);
	} else if (in_any_view(actor, FxZero, false)) {
		FLAG_ON(actor, FLG_BRO_ACTIVE);
		VAL(actor, BRO_TEMP2) = 101L;
		VAL(actor, BRO_TEMP3) = VAL(actor, BRO_TEMP5) = 1L;
	}

	if (VAL(actor, BRO_MOVE) < 0L) {
		++VAL(actor, BRO_MOVE);
		move_actor(actor, POS_ADD(actor, FfInt(-2L), FxZero));
	} else if (VAL(actor, BRO_MOVE) > 0L) {
		--VAL(actor, BRO_MOVE);
		move_actor(actor, POS_ADD(actor, FfInt(2L), FxZero));
	}

	if (VAL(actor, BRO_TEMP2) > -1L) {
		if (VAL(actor, BRO_TEMP3) == 1L) {
			VAL(actor, BRO_TEMP2) = 0L;
			VAL(actor, BRO_MOVE) = -32L - VAL(actor, BRO_TEMP4);
			VAL(actor, BRO_TEMP3) = 2L;
		} else if (VAL(actor, BRO_TEMP3) == 5L) {
			VAL(actor, BRO_TEMP2) = 0L;
			VAL(actor, BRO_MOVE) = 32L + VAL(actor, BRO_TEMP4);
			VAL(actor, BRO_TEMP3) = 6L;
		}
	}

	if (VAL(actor, BRO_MOVE) == 0L)
		switch (VAL(actor, BRO_TEMP3)) {
		default:
			break;
		case 2L:
			VAL(actor, BRO_TEMP3) = 3L;
			break;
		case 4L:
			VAL(actor, BRO_TEMP3) = 5L;
			break;
		case 6L:
			VAL(actor, BRO_TEMP3) = 7L;
			break;
		case 8L: {
			VAL(actor, BRO_TEMP3) = 1L;
			VAL(actor, BRO_TEMP4) = -rng(64L);
			VAL(actor, BRO_TEMP5) = 1L + rng(5L);
			break;
		}
		}

	if (VAL(actor, BRO_TEMP2) > 100L)
		switch (VAL(actor, BRO_TEMP3)) {
		default:
			break;
		case 3L: {
			VAL(actor, BRO_TEMP2) = 0L;
			VAL(actor, BRO_MOVE) = 32L + VAL(actor, BRO_TEMP4);
			VAL(actor, BRO_TEMP3) = 4L;
			break;
		}
		case 7L: {
			VAL(actor, BRO_TEMP2) = 0L;
			VAL(actor, BRO_MOVE) = -32L - VAL(actor, BRO_TEMP4);
			VAL(actor, BRO_TEMP3) = 8L;
			break;
		}
		}

	if ((game_state.time % 10L) == 0L && ANY_FLAG(actor, FLG_BRO_ACTIVE) && VAL(actor, BRO_JUMP_STATE) <= 0L) {
		VAL(actor, BRO_JUMP) = rng(20L);
		FLAG_ON(actor, FLG_BRO_JUMP);
	}

	FLAG_OFF(actor, FLG_BRO_TOP | FLG_BRO_BOTTOM);
	collide_actor(actor);

	if (ANY_FLAG(actor, FLG_BRO_JUMP)) {
		if (VAL(actor, BRO_JUMP) == 5L && ANY_FLAG(actor, FLG_BRO_BOTTOM | FLG_BRO_TOP)) {
			VAL(actor, BRO_JUMP_STATE) = 1L;
			VAL(actor, Y_SPEED) = FfInt(-8L);
		} else if (VAL(actor, BRO_JUMP) == 15L && ANY_FLAG(actor, FLG_BRO_TOP)) {
			VAL(actor, BRO_JUMP_STATE) = 2L;
			VAL(actor, BRO_DOWN) = actor->pos.y + FfInt(80L);
			VAL(actor, Y_SPEED) = FfInt(-3L);
		}

		FLAG_OFF(actor, FLG_BRO_JUMP);
		VAL(actor, BRO_JUMP) = 0L;
	}

	if (((game_state.time * 2L) % 5L) == 0L && in_any_view(actor, FxZero, false) && ANY_FLAG(actor, FLG_BRO_ACTIVE))
	{
		VAL(actor, BRO_THROW) = rng((game_state.flags & (GF_HARDCORE | GF_LOST | GF_FUNNY)) ? 11L : 20L);
		FLAG_ON(actor, FLG_BRO_TEMP30 | FLG_BRO_TEMP31);
	}

	if (VAL(actor, BRO_THROW) == 10L && ANY_FLAG(actor, FLG_BRO_TEMP30) && ANY_FLAG(actor, FLG_BRO_TEMP31)) {
		++VAL(actor, BRO_THROW_STATE);
		FLAG_OFF(actor, FLG_BRO_TEMP31);
	}

	if (VAL(actor, BRO_THROW_STATE) > 0L) {
		++VAL(actor, BRO_THROW_STATE);
		if (VAL(actor, BRO_THROW_STATE) > 30L) {
			VAL(actor, BRO_THROW_STATE) = VAL(actor, BRO_THROW) = 0L;
			FLAG_OFF(actor, FLG_BRO_TEMP30);

			GameActor* missile = create_actor(VAL(actor, BRO_MISSILE),
				POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FfInt(-9L) : FfInt(9L), FfInt(-20L)));
			if (missile != NULL)
				switch (missile->type) {
				default: {
					VAL(missile, X_SPEED)
						= FfInt((ANY_FLAG(actor, FLG_X_FLIP) ? -1L : 1L) * (1L + rng(5L)));
					VAL(missile, Y_SPEED) = FfInt(-6L - rng(5L));
					FLAG_ON(missile, actor->flags & FLG_X_FLIP);
					play_actor_sound(missile, "hammer");
					break;
				}

				case ACT_MISSILE_FIREBALL: {
					VAL(missile, X_SPEED) = ANY_FLAG(actor, FLG_X_FLIP) ? -532480L : 532480L;
					FLAG_ON(missile, actor->flags & FLG_X_FLIP);
					play_actor_sound(missile, "fire");
					break;
				}

				case ACT_MISSILE_SILVER_HAMMER: {
					VAL(missile, X_SPEED) = FfInt(
						ANY_FLAG(actor, FLG_X_FLIP) ? (-1L - rng(10L)) : (1L + rng(10L)));
					VAL(missile, Y_SPEED) = FfInt(-6L - rng(5L));
					FLAG_ON(missile, actor->flags & FLG_X_FLIP);
					play_actor_sound(missile, "hammer");
					break;
				}
				}
		}
	}

	VAL(actor, Y_SPEED) += 13107L;
	if (touching_solid(HITBOX_ADD(actor, FxZero, VAL(actor, Y_SPEED)), SOL_ALL)
		&& (VAL(actor, BRO_JUMP_STATE) == 0L
			|| (VAL(actor, BRO_JUMP_STATE) == 1L && VAL(actor, Y_SPEED) > FxZero)
			|| (VAL(actor, BRO_JUMP_STATE) == 2L && actor->pos.y > VAL(actor, BRO_DOWN))))
	{
		fixed i = VAL(actor, Y_SPEED),
		      jut = FfInt((int32_t)(VAL(actor, Y_SPEED) > FxZero) - (int32_t)(VAL(actor, Y_SPEED) < FxZero));

		while (i >= FxOne) {
			if (touching_solid(HITBOX_ADD(actor, FxZero, jut), SOL_ALL))
				break;
			move_actor(actor, POS_ADD(actor, FxZero, jut));
			i -= FxOne;
		}

		VAL(actor, Y_SPEED) = FxZero;
		VAL(actor, BRO_JUMP_STATE) = 0L;
	}

	move_actor(actor, POS_SPEED(actor));
}

static void draw(const GameActor* actor) {
	const char* tex;
	const uint8_t frame = (int)((float)game_state.time / 7.142857142857143f) % 2L;
	if (VAL(actor, BRO_THROW_STATE) > 0L)
		switch (VAL(actor, BRO_MISSILE)) {
		default:
			break;

		case ACT_MISSILE_HAMMER: {
			switch (frame) {
			default:
				tex = "enemies/bro_hammer";
				break;
			case 1L:
				tex = "enemies/bro_hammer2";
				break;
			}
			break;
		}

		case ACT_MISSILE_FIREBALL: {
			switch (frame) {
			default:
				tex = "enemies/bro_fire";
				break;
			case 1L:
				tex = "enemies/bro_fire2";
				break;
			}
			break;
		}

		case ACT_MISSILE_SILVER_HAMMER: {
			switch (frame) {
			default:
				tex = "enemies/bro_silver";
				break;
			case 1L:
				tex = "enemies/bro_silver2";
				break;
			}
			break;
		}
		}
	else
		switch (frame) {
		default:
			tex = "enemies/bro";
			break;
		case 1L:
			tex = "enemies/bro2";
			break;
		}

	draw_actor(actor, tex, 0, WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/bro", 0.f, WHITE);
}

static void cleanup(GameActor* actor) {
	decrease_ambush();
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, FfInt(-16L), 200L))
			kill_enemy(actor, from, false);
		else
			maybe_hit_player(actor, from);
		break;
	}

	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 200L);
		break;
	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
	case ACT_MISSILE_FIREBALL:
		hit_fireball(actor, from, 200L);
		break;
	case ACT_MISSILE_BEETROOT:
		hit_beetroot(actor, from, 200L);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 200L);
		break;
	}
}

const GameActorTable TAB_BRO = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_dead = draw_corpse,
	.cleanup = cleanup,
	.collide = collide,
};

// =========
// BRO LAYER
// =========

static void create_layer(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void collide_layer(GameActor* actor, GameActor* from) {
	if (from->type != ACT_BRO)
		return;
	if (ANY_FLAG(actor, FLG_BRO_LAYER_TOP))
		FLAG_ON(from, FLG_BRO_TOP);
	else
		FLAG_ON(from, FLG_BRO_BOTTOM);
}

const GameActorTable TAB_BRO_LAYER = {.create = create_layer, .collide = collide_layer};
