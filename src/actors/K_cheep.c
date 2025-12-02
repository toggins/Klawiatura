#include "K_string.h"

#include "actors/K_enemies.h"
#include "actors/K_player.h"

enum {
	VAL_BASS_EAT = VAL_CUSTOM,
	VAL_BASS_EAT_TIME,
};

enum {
	FLG_BASS_SWIM = CUSTOM_FLAG(0),
	FLG_BASS_ATTACK = CUSTOM_FLAG(1),
};

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
	load_texture_num("enemies/cheep_blue%u", 2L, false);

	load_sound("stomp", false);
	load_sound("kick", false);

	load_actor(ACT_POINTS);
}

static void tick_blue(GameActor* actor) {
	move_enemy(actor, (fvec2){FxOne, FxZero}, false);
}

static void draw_blue(const GameActor* actor) {
	draw_actor(actor, fmt("enemies/cheep_blue%u", (int)((float)game_state.time / 11.11111111111111f) % 2L), 0.f,
		B_WHITE);
}

static void draw_blue_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/cheep_blue0", 0.f, B_WHITE);
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
	load_texture_num("enemies/cheep_spiky%u", 2L, false);

	load_sound("kick", false);

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
	draw_actor(actor, fmt("enemies/cheep_spiky%u", (int)((float)game_state.time / 6.25f) % 2L), 0.f, B_WHITE);
}

static void draw_spiky_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/cheep_blue0", 0.f, B_WHITE);
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

// =========
// BOSS BASS
// =========

static void load_bass() {
	load_texture_num("enemies/bass%u", 4L, false);

	load_actor(ACT_WATER_SPLASH);
	load_actor(ACT_POINTS);
}

static void create_bass(GameActor* actor) {
	actor->box.start.x = FfInt(-20L);
	actor->box.start.y = FfInt(-64L);
	actor->box.end.x = FfInt(20L);

	VAL(actor, BASS_EAT) = NULLACT;
}

static void tick_bass(GameActor* actor) {
	GameActor* eat = get_actor(VAL(actor, BASS_EAT));
	if (eat == NULL) {
		GameActor* nearest = nearest_pawn(actor->pos);
		if (nearest != NULL) {
			if (ANY_FLAG(actor, FLG_BASS_SWIM)) {
				if (nearest->pos.x < actor->pos.x && VAL(actor, X_SPEED) > FfInt(-8L))
					VAL(actor, X_SPEED) -= 16384L;
				else if (nearest->pos.x > actor->pos.x && VAL(actor, X_SPEED) < FfInt(8L))
					VAL(actor, X_SPEED) += 16384L;

				if (nearest->pos.y < actor->pos.y
					&& (actor->pos.y - FfInt(20L)) > (game_state.water + FfInt(24L)))
					VAL(actor, Y_SPEED) -= FxHalf;
				else if (nearest->pos.y > actor->pos.y)
					VAL(actor, Y_SPEED) += FxHalf;
			}

			if ((!ANY_FLAG(actor, FLG_X_FLIP) && nearest->pos.x > actor->pos.x
				    && nearest->pos.x < (actor->pos.x + FfInt(128L)))
				|| (ANY_FLAG(actor, FLG_X_FLIP) && nearest->pos.x < actor->pos.x
					&& nearest->pos.x > (actor->pos.x - FfInt(128L))))
			{
				FLAG_ON(actor, FLG_BASS_ATTACK);
				if (nearest->pos.y < game_state.water && ANY_FLAG(actor, FLG_BASS_SWIM)
					&& VAL(actor, Y_SPEED) < FfInt(2L))
					VAL(actor, Y_SPEED) = -425984L;
			} else
				FLAG_OFF(actor, FLG_BASS_ATTACK);
		}
	}

	if (ANY_FLAG(actor, FLG_BASS_SWIM)) {
		if (VAL(actor, Y_SPEED) > FxZero)
			VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED) - 16384L, FxZero);
		else if (VAL(actor, Y_SPEED) < FxZero)
			VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) + 16384L, FxZero);

		if (game_state.water >= (actor->pos.y - FfInt(20L)))
			FLAG_OFF(actor, FLG_BASS_SWIM);
	} else {
		if (VAL(actor, X_SPEED) < FfInt(-6L))
			VAL(actor, X_SPEED) += 16384L;
		else if (VAL(actor, X_SPEED) > FfInt(6L))
			VAL(actor, X_SPEED) -= 16384L;
		VAL(actor, Y_SPEED) += 19005L;

		if (game_state.water < (actor->pos.y - FfInt(20L))) {
			if (VAL(actor, Y_SPEED) >= FfInt(4L))
				create_actor(ACT_WATER_SPLASH, POS_ADD(actor, FxZero, FfInt(-15L)));
			FLAG_ON(actor, FLG_BASS_SWIM);
		}
	}

	if (VAL(actor, X_SPEED) < FxZero)
		FLAG_ON(actor, FLG_X_FLIP);
	else if (VAL(actor, X_SPEED) > FxZero)
		FLAG_OFF(actor, FLG_X_FLIP);
	move_actor(actor, POS_SPEED(actor));

	eat = get_actor(VAL(actor, BASS_EAT));
	if (eat == NULL)
		VAL(actor, BASS_EAT) = NULLACT;
	else {
		move_actor(eat, actor->pos);
		GamePlayer* player = get_owner(eat);
		if (player != NULL)
			player->pos = actor->pos;

		if (++VAL(actor, BASS_EAT_TIME) > 30L) {
			kill_player(eat);
			VAL(actor, BASS_EAT) = NULLACT;
		}

		FLAG_OFF(actor, FLG_BASS_ATTACK);
	}
}

static void draw_bass(const GameActor* actor) {
	const char* tex = fmt(
		"enemies/bass%u", ((int)((float)game_state.time / 3.f) % 2L) + (ANY_FLAG(actor, FLG_BASS_ATTACK) * 2L));
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void draw_bass_corpse(const GameActor* actor) {
	draw_actor_offset(actor, "enemies/bass0", B_XY(0.f, -32.f), 0.f, B_WHITE);
}

static void cleanup_bass(GameActor* actor) {
	GameActor* eat = get_actor(VAL(actor, BASS_EAT));
	if (eat != NULL) {
		FLAG_ON(eat, FLG_VISIBLE);
		FLAG_OFF(eat, FLG_FREEZE);
	}
}

static void collide_bass(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (!ANY_FLAG(actor, FLG_BASS_ATTACK) || get_actor(VAL(actor, BASS_EAT)) != NULL
			|| (ANY_FLAG(actor, FLG_X_FLIP) && from->pos.x > actor->pos.x)
			|| (!ANY_FLAG(actor, FLG_X_FLIP) && from->pos.x < actor->pos.x)
			|| (from->pos.y < (actor->pos.y - FfInt(25L))))
			break;

		FLAG_ON(from, FLG_FREEZE);
		FLAG_OFF(from, FLG_VISIBLE);
		VAL(actor, BASS_EAT) = from->id;
		VAL(actor, BASS_EAT_TIME) = 0L;
		break;
	}

	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
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

const GameActorTable TAB_BOSS_BASS = {
	.load = load_bass,
	.create = create_bass,
	.tick = tick_bass,
	.draw = draw_bass,
	.draw_dead = draw_bass_corpse,
	.cleanup = cleanup_bass,
	.collide = collide_bass,
};
