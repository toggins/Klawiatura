#include "K_string.h"

#include "actors/K_koopa.h"
#include "actors/K_player.h"

// =====
// KOOPA
// =====

static void load() {
	load_texture_num("enemies/koopa%u", 2L, FALSE);
	load_texture("enemies/shell0", FALSE);
	load_texture_num("enemies/koopa_red%u", 2L, FALSE);
	load_texture("enemies/shell_red0", FALSE);

	load_sound("stomp", FALSE);
	load_sound("kick", FALSE);

	load_actor(ACT_KOOPA_SHELL);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = Int2Fx(-15L);
	actor->box.start.y = Int2Fx(-32L);
	actor->box.end.x = Int2Fx(15L);

	increase_ambush();
}

static void tick(GameActor* actor) {
	VAL_TICK(actor, KOOPA_MAYDAY);
	const Bool red = ANY_FLAG(actor, FLG_KOOPA_RED);
	move_enemy(actor, (FVec2){red ? Int2Fx(2L) : FxOne, 19005L}, red);
}

static void draw(const GameActor* actor) {
	const char* tex = ANY_FLAG(actor, FLG_KOOPA_RED)
	                          ? fmt("enemies/koopa_red%u", (int)((float)game_state.time / 11.11111111111111f) % 2L)
	                          : fmt("enemies/koopa%u", (int)((float)game_state.time / 16.66666666666667f) % 2L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/shell_red0" : "enemies/shell0", 0.f, B_WHITE);
}

static void cleanup(GameActor* actor) {
	decrease_ambush();
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (VAL(actor, KOOPA_MAYDAY) > 0L)
			break;

		if (check_stomp(actor, from, Int2Fx(-16L), 100L)) {
			GameActor* shell = create_actor(ACT_KOOPA_SHELL, actor->pos);
			if (shell != NULL) {
				VAL(shell, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
				FLAG_ON(shell, actor->flags & FLG_KOOPA_RED);
				align_interp(shell, actor);
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

const GameActorTable TAB_KOOPA = {.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_dead = draw_corpse,
	.cleanup = cleanup,
	.collide = collide};

// ===========
// KOOPA SHELL
// ===========

static void load_shell() {
	load_texture_num("enemies/shell%u", 4L, FALSE);
	load_texture_num("enemies/shell_red%u", 4L, FALSE);

	load_sound("stomp", FALSE);
	load_sound("kick", FALSE);

	load_actor(ACT_POINTS);
}

static void create_shell(GameActor* actor) {
	actor->box.start.x = Int2Fx(-15L);
	actor->box.start.y = Int2Fx(-32L);
	actor->box.end.x = Int2Fx(15L);

	VAL(actor, SHELL_PLAYER) = (ActorValue)NULLPLAY;
	VAL(actor, SHELL_HIT) = 6L;
}

static void tick_shell(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (ANY_FLAG(actor, FLG_SHELL_ACTIVE))
		VAL(actor, SHELL_FRAME) += 35389L;
	VAL_TICK(actor, SHELL_HIT);

	const Fixed spd = VAL(actor, X_SPEED);
	VAL(actor, Y_SPEED) += 24966L;

	displace_actor(actor, Int2Fx(10L), FALSE);
	collide_actor(actor);
	if (VAL(actor, X_TOUCH) != 0L)
		VAL(actor, X_SPEED) = VAL(actor, X_TOUCH) * -Fabs(spd);
}

static void draw_shell(const GameActor* actor) {
	const char* tex = fmt(ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/shell_red%u" : "enemies/shell%u",
		Fx2Int(VAL(actor, SHELL_FRAME)) % 4L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide_shell(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (VAL(from, PLAYER_STARMAN) > 0L) {
			player_starman(from, actor);
			break;
		}
		if (VAL(actor, SHELL_HIT) > 0L) {
			if (VAL(actor, SHELL_HIT) < 2L && VAL(actor, SHELL_PLAYER) == get_owner_id(from))
				++VAL(actor, SHELL_HIT);
			break;
		}

		if (!ANY_FLAG(actor, FLG_SHELL_ACTIVE)) {
			play_state_sound("kick", PLAY_POS, 0L, A_ACTOR(actor));
			VAL(actor, X_SPEED) = (from->pos.x > actor->pos.x) ? Int2Fx(-6L) : Int2Fx(6L);
			VAL(actor, SHELL_PLAYER) = (ActorValue)get_owner_id(from);
			VAL(actor, SHELL_HIT) = 6L;
			VAL(actor, SHELL_COMBO) = 0L;
			VAL(actor, SHELL_FRAME) = FxZero;
			FLAG_ON(actor, FLG_SHELL_ACTIVE);
			break;
		}

		if (check_stomp(actor, from, Int2Fx(-16L), 100L)) {
			VAL(actor, X_SPEED) = FxZero;
			VAL(actor, SHELL_PLAYER) = (ActorValue)NULLPLAY;
			VAL(actor, SHELL_HIT) = 6L;
			VAL(actor, SHELL_COMBO) = 0L;
			VAL(actor, SHELL_FRAME) = FxZero;
			FLAG_OFF(actor, FLG_SHELL_ACTIVE);
		} else
			maybe_hit_player(actor, from);
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

static PlayerID shell_owner(const GameActor* actor) {
	return (PlayerID)VAL(actor, SHELL_PLAYER);
}

const GameActorTable TAB_KOOPA_SHELL = {
	.load = load_shell,
	.create = create_shell,
	.tick = tick_shell,
	.draw = draw_shell,
	.draw_dead = draw_corpse,
	.collide = collide_shell,
	.owner = shell_owner,
};

// =========
// PARAKOOPA
// =========

static void load_parakoopa() {
	load_texture_num("enemies/parakoopa%u", 2L, FALSE);
	load_texture("enemies/shell0", FALSE);
	load_texture_num("enemies/parakoopa_red%u", 2L, FALSE);
	load_texture("enemies/shell_red0", FALSE);

	load_sound("stomp", FALSE);
	load_sound("kick", FALSE);

	load_actor(ACT_KOOPA);
	load_actor(ACT_POINTS);
}

static void tick_parakoopa(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PARAKOOPA_START)) {
		VAL(actor, PARAKOOPA_Y) = actor->pos.y;
		VAL(actor, PARAKOOPA_ANGLE) = FxRad(Int2Fx(rng(360L)));
		FLAG_ON(actor, FLG_PARAKOOPA_START);
	}

	move_actor(actor,
		(FVec2){actor->pos.x, VAL(actor, PARAKOOPA_Y) + Fmul(Int2Fx(50L), Fcos(VAL(actor, PARAKOOPA_ANGLE)))});
	VAL(actor, PARAKOOPA_ANGLE) += FxRad(Int2Fx(2L));

	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		if (nearest->pos.x < actor->pos.x)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (nearest->pos.x > actor->pos.x)
			FLAG_OFF(actor, FLG_X_FLIP);
	}
}

static void draw_parakoopa(const GameActor* actor) {
	const char* tex = fmt(ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/parakoopa_red%u" : "enemies/parakoopa%u",
		(int)((float)(game_state.time) / 8.333333333333333f) % 2L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide_parakoopa(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, Int2Fx(-16L), 100L)) {
			GameActor* koopa = create_actor(ACT_KOOPA, actor->pos);
			if (koopa != NULL) {
				VAL(koopa, KOOPA_MAYDAY) = 2L;
				FLAG_ON(koopa, actor->flags & (FLG_X_FLIP | FLG_KOOPA_RED));
				align_interp(koopa, actor);
			}
			mark_ambush_winner(from);
			FLAG_ON(actor, FLG_DESTROY);
		} else
			maybe_hit_player(actor, from);
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

const GameActorTable TAB_PARAKOOPA = {
	.load = load_parakoopa,
	.create = create,
	.tick = tick_parakoopa,
	.draw = draw_parakoopa,
	.draw_dead = draw_corpse,
	.cleanup = cleanup,
	.collide = collide_parakoopa,
};
