#include "actors/K_artillery.h"
#include "actors/K_enemies.h"
#include "actors/K_koopa.h"
#include "actors/K_lakitu.h"
#include "actors/K_missiles.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

// ================
// HELPER FUNCTIONS
// ================

void move_enemy(GameActor* actor, FVec2 speed, Bool edge) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (!ANY_FLAG(actor, FLG_ENEMY_ACTIVE) && in_any_view(actor, FxOne, FALSE)) {
		VAL(actor, X_SPEED) = ANY_FLAG(actor, FLG_X_FLIP) ? -speed.x : speed.x;
		FLAG_ON(actor, FLG_ENEMY_ACTIVE);
	}

	if (edge) {
		const Fixed x1 = ANY_FLAG(actor, FLG_X_FLIP) ? (actor->pos.x + actor->box.start.x - FxOne)
		                                             : (actor->pos.x + actor->box.end.x);
		const Fixed y1 = actor->pos.y + actor->box.start.y;
		const Fixed x2 = x1 + FxOne;
		const Fixed y2 = actor->pos.y + actor->box.end.y + FxOne;
		if (!touching_solid(
			    (FRect){
				    {x1, y1},
                                    {x2, y2}
                },
			    SOL_ALL))
		{
			if (VAL(actor, X_SPEED) < FxZero) {
				VAL(actor, X_SPEED) = speed.x;
				FLAG_OFF(actor, FLG_X_FLIP);
			} else if (VAL(actor, X_SPEED) > FxZero) {
				VAL(actor, X_SPEED) = -speed.x;
				FLAG_ON(actor, FLG_X_FLIP);
			}
		}
	}

	VAL(actor, Y_SPEED) += speed.y;
	displace_actor(actor, FxFrom(10L), FALSE);

	collide_actor(actor);
	VAL_TICK(actor, ENEMY_TURN);

	if (VAL(actor, X_TOUCH) != 0L) {
		VAL(actor, X_SPEED) = VAL(actor, X_TOUCH) * -speed.x;
		if (VAL(actor, X_SPEED) < FxZero)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (VAL(actor, X_SPEED) > FxZero)
			FLAG_OFF(actor, FLG_X_FLIP);
	}
}

void turn_enemy(GameActor* actor) {
	if (VAL(actor, ENEMY_TURN) > 0L) {
		++VAL(actor, ENEMY_TURN);
		return;
	}

	VAL(actor, X_SPEED) = -VAL(actor, X_SPEED);
	if (VAL(actor, X_SPEED) < FxZero)
		FLAG_ON(actor, FLG_X_FLIP);
	else if (VAL(actor, X_SPEED) > FxZero)
		FLAG_OFF(actor, FLG_X_FLIP);

	VAL(actor, ENEMY_TURN) = 2L;
}

GameActor* kill_enemy(GameActor* actor, GameActor* from, Bool kick) {
	if (actor == NULL)
		return NULL;

	switch (actor->type) {
	default:
		break;

	case ACT_PIRANHA_PLANT:
	case ACT_ROTODISC:
	case ACT_PODOBOO: {
		if (kick)
			play_state_sound("kick", PLAY_POS, 0L, A_ACTOR(actor));
		FLAG_ON(actor, FLG_DESTROY);
		mark_ambush_winner(from);
		return NULL;
	}

	case ACT_BULLET_BILL: {
		VAL(actor, X_SPEED) = Fmul(VAL(actor, X_SPEED), 45371L);
		VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
		FLAG_ON(actor, FLG_ARTILLERY_DEAD);

		if (kick)
			play_state_sound("kick", PLAY_POS, 0L, A_ACTOR(actor));
		return actor;
	}
	}

	GameActor* dead = create_actor(ACT_DEAD, POS_ADD(actor, FxZero, FxFrom(-32L)));
	if (dead == NULL)
		return NULL;

	VAL(dead, DEAD_TYPE) = actor->type;
	dead->flags = (actor->flags & (~(FLG_DESTROY | FLG_FREEZE))) | FLG_Y_FLIP;

	if (kick) {
		const Fixed rnd = FxFrom(rng(5L));
		const Fixed dir = 77208L + Fmul(rnd, 12868L);
		VAL(dead, X_SPEED) = Fmul(Fcos(dir), FxFrom(3L));
		VAL(dead, Y_SPEED) = Fmul(Fsin(dir), FxFrom(-3L));
		play_state_sound("kick", PLAY_POS, 0L, A_ACTOR(actor));
	}

	FLAG_ON(actor, FLG_DESTROY);
	mark_ambush_winner(from);
	return dead;
}

Bool check_stomp(GameActor* actor, GameActor* from, Fixed yoffs, Sint32 points) {
	if (actor == NULL || from == NULL || from->type != ACT_PLAYER || VAL(from, PLAYER_STARMAN) > 0L)
		return FALSE;

	if (from->pos.y < (actor->pos.y + yoffs) && (VAL(from, Y_SPEED) >= FxZero || ANY_FLAG(from, FLG_PLAYER_STOMP)))
	{
		GamePlayer* player = get_owner(from);
		VAL(from, Y_SPEED) = Fmin(VAL(from, Y_SPEED),
			(player != NULL && ANY_INPUT(player, GI_JUMP)) ? FxFrom(-13L) : FxFrom(-8L));
		FLAG_ON(from, FLG_PLAYER_STOMP);

		give_points(actor, player, points);
		play_state_sound("stomp", PLAY_POS, 0L, A_ACTOR(from));
		return TRUE;
	}

	return FALSE;
}

Bool maybe_hit_player(GameActor* actor, GameActor* from) {
	if (actor == NULL || from == NULL || from->type != ACT_PLAYER)
		return FALSE;
	if (VAL(from, PLAYER_STARMAN) > 0L) {
		player_starman(from, actor);
		return FALSE;
	}
	return hit_player(from);
}

Bool hit_shell(GameActor* actor, GameActor* from) {
	if (actor == NULL || from == NULL || (from->type != ACT_KOOPA_SHELL && from->type != ACT_BUZZY_SHELL)
		|| !ANY_FLAG(from, FLG_SHELL_ACTIVE))
		return FALSE;

	if ((actor->type == ACT_KOOPA_SHELL || actor->type == ACT_BUZZY_SHELL) && ANY_FLAG(actor, FLG_SHELL_ACTIVE)) {
		VAL(actor, SHELL_COMBO) = VAL(from, SHELL_COMBO) = 0L;
		give_points(from, get_owner(from), 100L);
		kill_enemy(from, from, TRUE);
	}

	Sint32 points = 0L;
	switch (VAL(from, SHELL_COMBO)++) {
	case 0L:
		points = 100L;
		break;
	case 1L:
		points = 200L;
		break;
	case 2L:
		points = 500L;
		break;
	case 3L:
		points = 1000L;
		break;
	case 4L:
		points = 2000L;
		break;
	case 5L:
		points = 5000L;
		break;

	default: {
		points = -1L;
		VAL(from, SHELL_COMBO) = 0L;
		break;
	}
	}
	give_points(actor, get_owner(from), points);

	kill_enemy(actor, from, TRUE);
	return TRUE;
}

void hit_bump(GameActor* actor, GameActor* from, Sint32 points) {
	if (actor == NULL || from == NULL)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	give_points(actor, player, points);
	kill_enemy(actor, from, TRUE);
}

void block_fireball(GameActor* actor) {
	if (actor == NULL || get_owner(actor) == NULL)
		return;
	play_state_sound("bump", PLAY_POS, 0L, A_ACTOR(actor));
	FLAG_ON(actor, FLG_DESTROY);
}

void hit_fireball(GameActor* actor, GameActor* from, Sint32 points) {
	if (actor == NULL || from == NULL)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	give_points(actor, player, points);
	kill_enemy(actor, from, TRUE);
	FLAG_ON(from, FLG_DESTROY);
}

void block_beetroot(GameActor* actor) {
	if (actor == NULL || get_owner(actor) == NULL)
		return;
	play_state_sound("bump", PLAY_POS, 0L, A_ACTOR(actor));
	FLAG_ON(actor, FLG_MISSILE_HIT);
}

void hit_beetroot(GameActor* actor, GameActor* from, Sint32 points) {
	if (actor == NULL || from == NULL)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	give_points(actor, player, points);
	kill_enemy(actor, from, TRUE);
	FLAG_ON(from, FLG_MISSILE_HIT);
}

void hit_hammer(GameActor* actor, GameActor* from, Sint32 points) {
	hit_bump(actor, from, points);
}

void mark_ambush_winner(GameActor* actor) {
	if (game_state.sequence.type != SEQ_AMBUSH || game_state.sequence.time <= 0L)
		return;
	GamePlayer* player = get_owner(actor);
	if (player != NULL)
		game_state.sequence.activator = player->id;
}

void increase_ambush() {
	if (game_state.sequence.type == SEQ_AMBUSH)
		++game_state.sequence.time;
}

void decrease_ambush() {
	if (game_state.sequence.type == SEQ_AMBUSH && game_state.sequence.time > 0L)
		--game_state.sequence.time;
}

// ==========
// DEAD ENEMY
// ==========

static void create_dead(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FxFrom(-32L);
	actor->box.end.x = actor->box.end.y = FxFrom(32L);
}

static void tick_dead(GameActor* actor) {
	if (!in_any_view(actor, FxZero, TRUE)) {
		switch (VAL(actor, DEAD_TYPE)) {
		default:
			break;

		case ACT_LAKITU: {
			if (++VAL(actor, DEAD_RESPAWN) <= 300L)
				return;
			GameActor* lakitu = create_actor(
				ACT_LAKITU, (FVec2){game_state.size.x + FxFrom(200L), FxFrom(57L + rng(31L))});
			if (lakitu != NULL) {
				FLAG_ON(lakitu, actor->flags & FLG_LAKITU_GREEN);
				break;
			}
			return;
		}

		case ACT_BOSS_BASS: {
			if (++VAL(actor, DEAD_RESPAWN) <= 300L)
				return;
			if (create_actor(ACT_BOSS_BASS,
				    (FVec2){game_state.size.x + FxFrom(200L), game_state.water + FxFrom(32L)})
				!= NULL)
				break;
			return;
		}
		}

		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, Y_SPEED) += 13107L;
	move_actor(actor, POS_SPEED(actor));
}

const GameActorTable TAB_DEAD = {.create = create_dead, .tick = tick_dead, .draw = draw_dead};
