
#include "K_string.h"
#include "K_tick.h"

#include "actors/K_autoscroll.h"
#include "actors/K_enemies.h"
#include "actors/K_missiles.h" // IWYU pragma: keep
#include "actors/K_platform.h"
#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_warp.h"

#define GROUND_TIME 5L

// ================
// HELPER FUNCTIONS
// ================

static void load_player_textures() {
	load_texture("player/mario/dead", FALSE);
	load_texture_num("player/mario/grow%u", 3L, FALSE);

	load_texture("player/mario/small/idle", FALSE);
	load_texture_num("player/mario/small/walk%u", 2L, FALSE);
	load_texture("player/mario/small/jump", FALSE);
	load_texture_num("player/mario/small/swim%u", 4L, FALSE);

	load_texture("player/mario/big/idle", FALSE);
	load_texture_num("player/mario/big/walk%u", 2L, FALSE);
	load_texture("player/mario/big/jump", FALSE);
	load_texture("player/mario/big/duck", FALSE);
	load_texture_num("player/mario/big/swim%u", 4L, FALSE);

	load_texture("player/mario/fire/idle", FALSE);
	load_texture_num("player/mario/fire/walk%u", 2L, FALSE);
	load_texture("player/mario/fire/jump", FALSE);
	load_texture("player/mario/fire/duck", FALSE);
	load_texture("player/mario/fire/fire", FALSE);
	load_texture_num("player/mario/fire/swim%u", 4L, FALSE);

	load_texture("player/mario/beetroot/idle", FALSE);
	load_texture_num("player/mario/beetroot/walk%u", 2L, FALSE);
	load_texture("player/mario/beetroot/jump", FALSE);
	load_texture("player/mario/beetroot/duck", FALSE);
	load_texture("player/mario/beetroot/fire", FALSE);
	load_texture_num("player/mario/beetroot/swim%u", 4L, FALSE);

	load_texture("player/mario/lui/idle", FALSE);
	load_texture_num("player/mario/lui/walk%u", 2L, FALSE);
	load_texture("player/mario/lui/jump", FALSE);
	load_texture("player/mario/lui/duck", FALSE);
	load_texture_num("player/mario/lui/swim%u", 4L, FALSE);

	load_texture("player/mario/hammer/idle", FALSE);
	load_texture_num("player/mario/hammer/walk%u", 2L, FALSE);
	load_texture("player/mario/hammer/jump", FALSE);
	load_texture("player/mario/hammer/duck", FALSE);
	load_texture("player/mario/hammer/fire", FALSE);
	load_texture_num("player/mario/hammer/swim%u", 4L, FALSE);
}

PlayerFrame get_player_frame(const GameActor* actor) {
	if (VAL(actor, PLAYER_POWER) > 0L) {
		const GamePlayer* player = get_owner(actor);
		const ActorValue frame = VAL(actor, PLAYER_POWER) / 100L;

		switch ((player == NULL) ? POW_SMALL : player->power) {
		case POW_SMALL:
		case POW_BIG: {
			switch (frame % 3L) {
			default:
				return PF_GROW1;
			case 1L:
				return PF_GROW2;
			case 2L:
				return PF_GROW3;
			}
			break;
		}

		default: {
			switch (frame % 4L) {
			default:
				return PF_GROW1;
			case 1L:
				return PF_GROW2;
			case 2L:
				return PF_GROW3;
			case 3L:
				return PF_GROW4;
			}
			break;
		}
		}
	}

	const GameActor* warp = get_actor(VAL(actor, PLAYER_WARP));
	if (warp != NULL)
		switch (VAL(warp, WARP_ANGLE)) {
		case 0L:
		case 2L:
			switch (Fx2Int(VAL(actor, PLAYER_FRAME)) % 3L) {
			default:
				return PF_WALK1;
			case 1L:
				return PF_WALK2;
			case 2L:
				return PF_WALK3;
			}
		case 1L:
			return PF_JUMP;
		default:
			return PF_DUCK;
		}

	if (ANY_FLAG(actor, FLG_PLAYER_DUCK))
		return PF_DUCK;

	if (VAL(actor, PLAYER_GROUND) <= 0L) {
		if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			const Sint32 frame = Fx2Int(VAL(actor, PLAYER_FRAME));
			switch (frame) {
			case 0L:
			case 3L:
				return PF_SWIM1;
			case 1L:
			case 4L:
				return PF_SWIM2;
			case 2L:
			case 5L:
				return PF_SWIM3;
			default:
				return (frame % 2L) ? PF_SWIM5 : PF_SWIM4;
			}
		} else
			return (VAL(actor, Y_SPEED) < FxZero) ? PF_JUMP : PF_FALL;
	}

	if (VAL(actor, PLAYER_FIRE) > 0L)
		return PF_FIRE;

	if (VAL(actor, X_SPEED) != FxZero)
		switch (Fx2Int(VAL(actor, PLAYER_FRAME)) % 3L) {
		default:
			return PF_WALK1;
		case 1L:
			return PF_WALK2;
		case 2L:
			return PF_WALK3;
		}

	return PF_IDLE;
}

const char* get_player_texture(PlayerPower power, PlayerFrame frame) {
	if (frame == PF_DEAD)
		return "player/mario/dead";

	switch (power) {
	default:
	case POW_SMALL: {
		switch (frame) {
		default:
			return "player/mario/small/idle";

		case PF_WALK1:
			return "player/mario/small/walk0";
		case PF_WALK2:
			return "player/mario/small/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/small/jump";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/small/swim0";
		case PF_SWIM2:
			return "player/mario/small/swim1";
		case PF_SWIM3:
			return "player/mario/small/swim2";
		case PF_SWIM5:
			return "player/mario/small/swim3";

		case PF_GROW2:
			return "player/mario/grow0";
		case PF_GROW3:
			return "player/mario/big/walk1";
		}
		break;
	}

	case POW_BIG: {
		switch (frame) {
		default:
			return "player/mario/big/idle";

		case PF_WALK1:
			return "player/mario/big/walk0";
		case PF_WALK2:
		case PF_GROW1:
			return "player/mario/big/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/big/jump";

		case PF_DUCK:
			return "player/mario/big/duck";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/big/swim0";
		case PF_SWIM2:
			return "player/mario/big/swim1";
		case PF_SWIM3:
			return "player/mario/big/swim2";
		case PF_SWIM5:
			return "player/mario/big/swim3";

		case PF_GROW2:
			return "player/mario/grow0";
		case PF_GROW3:
			return "player/mario/small/idle";
		}
		break;
	}

	case POW_FIRE: {
		switch (frame) {
		default:
			return "player/mario/fire/idle";

		case PF_WALK1:
			return "player/mario/fire/walk0";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/fire/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/fire/jump";

		case PF_DUCK:
			return "player/mario/fire/duck";

		case PF_FIRE:
			return "player/mario/fire/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/fire/swim0";
		case PF_SWIM2:
			return "player/mario/fire/swim1";
		case PF_SWIM3:
			return "player/mario/fire/swim2";
		case PF_SWIM5:
			return "player/mario/fire/swim3";

		case PF_GROW1:
			return "player/mario/big/walk1";
		case PF_GROW2:
			return "player/mario/grow1";
		case PF_GROW4:
			return "player/mario/grow2";
		}
		break;
	}

	case POW_BEETROOT: {
		switch (frame) {
		default:
			return "player/mario/beetroot/idle";

		case PF_WALK1:
			return "player/mario/beetroot/walk0";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/beetroot/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/beetroot/jump";

		case PF_DUCK:
			return "player/mario/beetroot/duck";

		case PF_FIRE:
			return "player/mario/beetroot/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/beetroot/swim0";
		case PF_SWIM2:
			return "player/mario/beetroot/swim1";
		case PF_SWIM3:
			return "player/mario/beetroot/swim2";
		case PF_SWIM5:
			return "player/mario/beetroot/swim3";

		case PF_GROW1:
			return "player/mario/big/walk1";
		case PF_GROW2:
			return "player/mario/grow1";
		case PF_GROW4:
			return "player/mario/grow2";
		}
		break;
	}

	case POW_LUI: {
		switch (frame) {
		default:
			return "player/mario/lui/idle";

		case PF_WALK1:
			return "player/mario/lui/walk0";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/lui/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/lui/jump";

		case PF_DUCK:
			return "player/mario/lui/duck";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/lui/swim0";
		case PF_SWIM2:
			return "player/mario/lui/swim1";
		case PF_SWIM3:
			return "player/mario/lui/swim2";
		case PF_SWIM5:
			return "player/mario/lui/swim3";

		case PF_GROW1:
			return "player/mario/big/walk1";
		case PF_GROW2:
			return "player/mario/grow1";
		case PF_GROW4:
			return "player/mario/grow2";
		}
		break;
	}

	case POW_HAMMER: {
		switch (frame) {
		default:
			return "player/mario/hammer/idle";

		case PF_WALK1:
			return "player/mario/hammer/walk0";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/hammer/walk1";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/hammer/jump";

		case PF_DUCK:
			return "player/mario/hammer/duck";

		case PF_FIRE:
			return "player/mario/hammer/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/hammer/swim0";
		case PF_SWIM2:
			return "player/mario/hammer/swim1";
		case PF_SWIM3:
			return "player/mario/hammer/swim2";
		case PF_SWIM5:
			return "player/mario/hammer/swim3";

		case PF_GROW1:
			return "player/mario/big/walk1";
		case PF_GROW2:
			return "player/mario/grow1";
		case PF_GROW4:
			return "player/mario/grow2";
		}
		break;
	}
	}
}

Bool hit_player(GameActor* actor) {
	if (actor == NULL || actor->type != ACT_PLAYER || VAL(actor, PLAYER_FLASH) > 0L
		|| VAL(actor, PLAYER_STARMAN) > 0L || get_actor(VAL(actor, PLAYER_WARP)) != NULL
		|| game_state.sequence.type == SEQ_WIN)
		return FALSE;

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		if (player->power == POW_SMALL) {
			kill_player(actor);
			return TRUE;
		}
		player->power = (player->power == POW_BIG) ? POW_SMALL : POW_BIG;
	}

	VAL(actor, PLAYER_POWER) = 3000L, VAL(actor, PLAYER_FLASH) = 100L;
	play_state_sound("warp", PLAY_POS, 0L, A_ACTOR(actor));
	return TRUE;
}

void kill_player(GameActor* actor) {
	if (actor->type != ACT_PLAYER)
		return;
	if (game_state.flags & GF_HUB) {
		respawn_player(get_owner(actor));
		return;
	}

	GameActor* dead = create_actor(ACT_PLAYER_DEAD, actor->pos);
	if (dead == NULL)
		return;
	if (!ANY_FLAG(actor, FLG_VISIBLE) && ANY_FLAG(actor, FLG_FREEZE))
		FLAG_OFF(dead, FLG_VISIBLE);
	align_interp(dead, actor);
	lerp_camera(1.f);

	// !!! CLIENT-SIDE !!!
	if (localplayer() == VAL(actor, PLAYER_INDEX) && VAL(actor, PLAYER_STARMAN) > 0L)
		stop_state_track(TS_POWER);
	// !!! CLIENT-SIDE !!!

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		VAL(dead, PLAYER_INDEX) = (ActorValue)player->id;

		if (player->lives >= 0L)
			--player->lives;
		player->power = POW_SMALL;

		GameActor* kevin = get_actor(player->kevin.actor);
		if (kevin != NULL) {
			create_actor(ACT_EXPLODE, POS_ADD(kevin, FxZero, FxFrom(-16L)));
			FLAG_ON(kevin, FLG_DESTROY);
		}
		player->kevin.actor = NULLACT;

		if (numplayers() > 1L && player->lives < 0L)
			hud_message(fmt("%s is out of lives!", get_player_name(player->id)));
	}

	Bool all_dead = TRUE;
	switch (game_state.sequence.type) {
	default: {
		if (game_state.clock == 0L || (game_state.flags & GF_SINGLE))
			break;

		// Check specific warps since these cause players to despawn and spectate the warper.
		GameActor* warp = get_actor(VAL(actor, PLAYER_WARP));
		if (warp != NULL && (ANY_FLAG(warp, FLG_WARP_EXIT) || VAL(warp, WARP_STRING) != 0L))
			break;

		for (PlayerID i = 0L; i < numplayers(); i++) {
			GamePlayer* survivor = get_player(i);
			if (survivor != NULL && (survivor->lives >= 0L || (game_state.flags & GF_HELL))) {
				all_dead = FALSE;
				break;
			}
		}
		break;
	}

	case SEQ_LOSE:
	case SEQ_WIN:
	case SEQ_SECRET:
		break;
	}
	if (all_dead) {
		FLAG_ON(dead, FLG_PLAYER_DEAD);
		if (!(game_state.flags & GF_SINGLE)
			&& (game_state.sequence.type == SEQ_WIN || game_state.sequence.type == SEQ_SECRET)
			&& rng(100L) == 0L)
			FLAG_ON(dead, FLG_PLAYER_JACKASS);

		game_state.sequence.type = SEQ_LOSE;
		game_state.sequence.time = 0L;
	}

	FLAG_ON(actor, FLG_DESTROY);
}

void win_player(GameActor* actor) {
	if (actor->type != ACT_PLAYER || game_state.sequence.type == SEQ_WIN)
		return;

	GamePlayer* player = get_owner(actor);
	if (player == NULL)
		return;

	const Bool was_bowser = (game_state.sequence.type == SEQ_BOWSER_END);
	game_state.sequence.type = SEQ_WIN;
	game_state.sequence.time = 1L;
	game_state.sequence.activator = player->id;

	for (PlayerID i = 0L; i < numplayers(); i++) {
		GamePlayer* p = get_player(i);
		if (p == NULL)
			continue;

		if (p != player) {
			GameActor* pawn = get_actor(p->actor);
			if (pawn != NULL)
				FLAG_ON(pawn, FLG_DESTROY);
			p->actor = NULLACT;
		}

		GameActor* kevin = get_actor(p->kevin.actor);
		if (kevin != NULL) {
			create_actor(ACT_EXPLODE, (FVec2){kevin->pos.x, kevin->pos.y - FxFrom(16L)});
			FLAG_ON(kevin, FLG_DESTROY);
		}
		p->kevin.actor = NULLACT;
	}

	for (ActorID i = 0L; i < MAX_MISSILES; i++) {
		GameActor* missile = get_actor(player->missiles[i]);
		if (missile == NULL)
			goto clear_missile;

		Sint32 points = 100L;
		switch (missile->type) {
		default:
			break;
		case ACT_MISSILE_BEETROOT:
			points = 200L;
			break;
		case ACT_MISSILE_HAMMER:
			points = 500L;
			break;
		}
		give_points(missile, player, points);
		FLAG_ON(missile, FLG_DESTROY);
	clear_missile:
		player->missiles[i] = NULLACT;
	}

	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL && !ANY_FLAG(autoscroll, FLG_SCROLL_TANKS | FLG_SCROLL_BOWSER)) {
		VAL(autoscroll, X_SPEED) = Fmul(VAL(autoscroll, X_SPEED), FxFrom(4L));
		VAL(autoscroll, Y_SPEED) = Fmul(VAL(autoscroll, Y_SPEED), FxFrom(4L));
	}

	game_state.pswitch = 0L;
	VAL(actor, PLAYER_FLASH) = VAL(actor, PLAYER_STARMAN) = 0L;
	set_view_player(player);

	for (TrackSlots i = 0L; i < (TrackSlots)TS_SIZE; i++)
		stop_state_track(i);
	play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? (was_bowser ? "win3" : "win2") : "win", 0L, 0L);
}

void player_starman(GameActor* actor, GameActor* from) {
	if (actor == NULL || from == NULL)
		return;

	Sint32 points = 0L;
	switch (VAL(actor, PLAYER_STARMAN_COMBO)++) {
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
		VAL(actor, PLAYER_STARMAN_COMBO) = 0L;
		break;
	}
	}
	give_points(from, get_owner(actor), points);

	create_actor(ACT_EXPLODE,
		(FVec2){Flerp(Fadd(from->pos.x, from->box.start.x), Fadd(from->pos.x, from->box.end.x), FxHalf),
			Flerp(Fadd(from->pos.y, from->box.start.y), Fadd(from->pos.y, from->box.end.y), FxHalf)});
	kill_enemy(from, actor, TRUE);
}

// ============
// PLAYER SPAWN
// ============

static void load_spawn() {
	load_actor(ACT_PLAYER);
}

static void create_spawn(GameActor* actor) {
	game_state.spawn = actor->id;
}

static void tick_spawn(GameActor* actor) {
	if ((game_state.flags & GF_HUB) || !(game_state.flags & GF_FRED) || ANY_FLAG(actor, FLG_PLAYER_FRED)
		|| (VAL(actor, PLAYER_SPAWN_FRED) <= 0L
			&& !(game_state.spawn == actor->id || in_any_view(actor, FxZero, FALSE))))
		return;
	if (++VAL(actor, PLAYER_SPAWN_FRED) < 50L)
		return;

	FVec2 pos = actor->pos;
	if (game_state.spawn == actor->id) {
		GameActor* checkpoint = get_actor(game_state.checkpoint);
		if (checkpoint != NULL)
			pos = checkpoint->pos;
	}

	create_actor(ACT_FRED, pos);
	FLAG_ON(actor, FLG_PLAYER_FRED);
}

static void cleanup_spawn(GameActor* actor) {
	if (game_state.spawn == actor->id)
		game_state.spawn = NULLACT;
}

const GameActorTable TAB_PLAYER_SPAWN = {
	.load = load_spawn,
	.create = create_spawn,
	.tick = tick_spawn,
	.cleanup = cleanup_spawn,
};

// ======
// PLAYER
// ======

static void load() {
	load_player_textures();

	load_sound("jump", FALSE);
	load_sound("fire", FALSE);
	load_sound("swim", FALSE);
	load_sound("warp", FALSE);
	load_sound("starman", FALSE);
	load_sound("bump", FALSE);
	load_sound("stomp", FALSE);
	load_sound("respawn", FALSE);

	load_track("win", FALSE);
	load_track("win2", FALSE);
	load_track("win3", FALSE);

	load_actor(ACT_PLAYER_EFFECT);
	load_actor(ACT_PLAYER_DEAD);
	load_actor(ACT_WATER_SPLASH);
	load_actor(ACT_BUBBLE);
	load_actor(ACT_MISSILE_FIREBALL);
	load_actor(ACT_MISSILE_BEETROOT);
	load_actor(ACT_MISSILE_HAMMER);
	load_actor(ACT_POINTS);

	if (game_state.flags & GF_KEVIN)
		load_actor(ACT_KEVIN);
	if (game_state.flags & GF_FRED)
		load_actor(ACT_FRED);
}

static void create(GameActor* actor) {
	actor->depth = -FxOne;

	actor->box.start.x = FxFrom(-9L);
	actor->box.start.y = FxFrom(-26L);
	actor->box.end.x = FxFrom(10L);

	VAL(actor, PLAYER_INDEX) = (ActorValue)NULLPLAY;
	VAL(actor, PLAYER_GROUND) = GROUND_TIME;
	VAL(actor, PLAYER_WARP) = NULLACT;
	VAL(actor, PLAYER_PLATFORM) = NULLACT;
}

static void tick(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (ANY_FLAG(actor, FLG_PLAYER_RESPAWN)) {
		if (touching_solid(HITBOX(actor), SOL_SOLID)) {
			move_actor(actor, (FVec2){Flerp(game_state.bounds.start.x, game_state.bounds.end.x, FxHalf),
						  actor->pos.y + FxOne});
			return;
		} else
			FLAG_OFF(actor, FLG_PLAYER_RESPAWN);
	}

	GameActor* warp = get_actor(VAL(actor, PLAYER_WARP));
	if (warp != NULL) {
		if (ANY_FLAG(actor, FLG_PLAYER_WARP_OUT)) {
			if (ANY_FLAG(warp, FLG_WARP_EXIT | FLG_WARP_SECRET) || VAL(warp, WARP_STRING) != 0L)
				goto skip_physics;

			switch (VAL(warp, WARP_ANGLE_OUT)) {
			case 0L: {
				move_actor(actor, POS_ADD(actor, -FxOne, FxZero));
				VAL(actor, PLAYER_FRAME) += 7864L;
				FLAG_ON(actor, FLG_X_FLIP);
				break;
			}

			case 1L:
				move_actor(actor, POS_ADD(actor, FxZero, FxOne));
				break;

			case 2L: {
				move_actor(actor, POS_ADD(actor, FxOne, FxZero));
				VAL(actor, PLAYER_FRAME) += 7864L;
				FLAG_OFF(actor, FLG_X_FLIP);
				break;
			}

			default:
				move_actor(actor, POS_ADD(actor, FxZero, -FxOne));
				break;
			}

			if (!touching_solid(HITBOX(actor), SOL_SOLID)) {
				actor->depth = -FxOne;
				VAL(actor, PLAYER_GROUND) = 2L;
				VAL(actor, PLAYER_WARP) = NULLACT;
				VAL(actor, PLAYER_WARP_STATE) = 0L;
				FLAG_OFF(actor, FLG_PLAYER_WARP_OUT);
			}
		} else {
			switch (VAL(warp, WARP_ANGLE)) {
			case 0L: {
				move_actor(actor, POS_ADD(actor, FxOne, FxZero));
				VAL(actor, PLAYER_FRAME) += 7864L;
				FLAG_OFF(actor, FLG_X_FLIP);
				break;
			}

			case 1L:
				move_actor(actor, POS_ADD(actor, FxZero, -FxOne));
				break;

			case 2L: {
				move_actor(actor, POS_ADD(actor, -FxOne, FxZero));
				VAL(actor, PLAYER_FRAME) += 7864L;
				FLAG_ON(actor, FLG_X_FLIP);
				break;
			}

			default:
				move_actor(actor, POS_ADD(actor, FxZero, FxOne));
				break;
			}

			++VAL(actor, PLAYER_WARP_STATE);
			if (VAL(actor, PLAYER_WARP_STATE) > 60L) {
				if (ANY_FLAG(warp, FLG_WARP_EXIT)) {
					win_player(actor);
					FLAG_ON(actor, FLG_PLAYER_WARP_OUT);
					goto skip_physics;
				} else if (VAL(warp, WARP_STRING) != 0L) {
					if (ANY_FLAG(warp, FLG_WARP_SECRET)) {
						game_state.sequence.type = SEQ_SECRET;
						game_state.sequence.time = 0L;

						hud_message(fmt("%s found a secret!",
							(numplayers() <= 1L) ? "You" : get_player_name(player->id)));
						for (TrackSlots i = 0L; i < (TrackSlots)TS_SIZE; i++)
							stop_state_track(i);
						play_state_track(TS_FANFARE, "warp", 0L, 0L);
					} else
						game_state.flags |= GF_END;

					FLAG_ON(actor, FLG_PLAYER_WARP_OUT);
					goto skip_physics;
				}

				if (VAL(warp, WARP_BOUNDS_X1) != VAL(warp, WARP_BOUNDS_X2)
					&& VAL(warp, WARP_BOUNDS_Y1) != VAL(warp, WARP_BOUNDS_Y2))
				{
					player->bounds.start.x = VAL(warp, WARP_BOUNDS_X1);
					player->bounds.start.y = VAL(warp, WARP_BOUNDS_Y1);
					player->bounds.end.x = VAL(warp, WARP_BOUNDS_X2);
					player->bounds.end.y = VAL(warp, WARP_BOUNDS_Y2);
				}

				FVec2 wpos = {VAL(warp, WARP_X), VAL(warp, WARP_Y)};
				switch (VAL(warp, WARP_ANGLE)) {
				default:
					break;
				case 3L:
					wpos.y += FxFrom(50L);
					break;
				}
				move_actor(actor, wpos);

				FLAG_ON(actor, FLG_PLAYER_WARP_OUT);
				skip_interp(actor);
				play_state_sound("warp", PLAY_POS, 0L, A_ACTOR(actor));
			}
		}

		goto skip_physics;
	} else
		VAL(actor, PLAYER_WARP) = NULLACT;

	if (game_state.sequence.type == SEQ_WIN) {
		player->input = 0L;
		if (VAL(actor, X_SPEED) < FxZero)
			player->input |= GI_RIGHT;
		else
			VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED), 163840L);
	}

	const ActorID pfid = (ActorID)VAL(actor, PLAYER_PLATFORM);
	GameActor* platform = get_actor(pfid);
	if (platform != NULL) {
		const Fixed vx = VAL(actor, X_SPEED), vy = VAL(actor, Y_SPEED),
			    pvx = platform->pos.x - VAL(platform, PLATFORM_X_FROM),
			    pvy = platform->pos.y - VAL(platform, PLATFORM_Y_FROM);

		VAL(actor, X_SPEED) = pvx, VAL(actor, Y_SPEED) = pvy;
		displace_actor(actor, FxZero, FALSE);
		VAL(actor, X_SPEED) = vx, VAL(actor, Y_SPEED) = vy;

		FRect mbox = HITBOX_ADD(actor, pvx, pvy);
		mbox.end.y += FxOne;
		const FRect pbox = HITBOX(platform);
		VAL(actor, PLAYER_PLATFORM) = Rcollide(mbox, pbox) ? pfid : NULLACT;
	}

	Fixed foot_y = actor->pos.y - FxOne;
	const Bool cant_run = (!ANY_INPUT(player, GI_RUN) || foot_y >= game_state.water),
		   jumped = ANY_PRESSED(player, GI_JUMP);

	if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) >= FxZero
		&& ((cant_run && VAL(actor, X_SPEED) < 286720L) || (!cant_run && VAL(actor, X_SPEED) < 491520L)))
		VAL(actor, X_SPEED) += 8192L;
	if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) <= FxZero
		&& ((cant_run && VAL(actor, X_SPEED) > -286720) || (!cant_run && VAL(actor, X_SPEED) > -491520L)))
		VAL(actor, X_SPEED) -= 8192L;

	if (!ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_SPEED) > FxZero)
		VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - 8192L, FxZero);
	if (!ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) < FxZero)
		VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + 8192L, FxZero);

	if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) < FxZero)
		VAL(actor, X_SPEED) += 24576L;
	if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) > FxZero)
		VAL(actor, X_SPEED) -= 24576L;

	if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) < FxOne && VAL(actor, X_SPEED) >= FxZero)
		VAL(actor, X_SPEED) += FxOne;
	if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& VAL(actor, X_SPEED) > -FxOne && VAL(actor, X_SPEED) <= FxZero)
		VAL(actor, X_SPEED) -= FxOne;

	if (cant_run && Fabs(VAL(actor, X_SPEED)) > 286720L)
		VAL(actor, X_SPEED) -= (VAL(actor, X_SPEED) >= FxZero) ? 8192L : -8192L;

	if (ANY_INPUT(player, GI_DOWN) && VAL(actor, PLAYER_GROUND) > 0L && player->power != POW_SMALL)
		FLAG_ON(actor, FLG_PLAYER_DUCK);
	if (ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
		if (!ANY_INPUT(player, GI_DOWN) || player->power == POW_SMALL || VAL(actor, PLAYER_GROUND) <= 0L)
			FLAG_OFF(actor, FLG_PLAYER_DUCK);
		else if (VAL(actor, X_SPEED) > FxZero)
			VAL(actor, X_SPEED)
				= Fmax(VAL(actor, X_SPEED) - (ANY_INPUT(player, GI_RIGHT) ? 18432L : 24576L), FxZero);
		else if (VAL(actor, X_SPEED) < FxZero)
			VAL(actor, X_SPEED)
				= Fmin(VAL(actor, X_SPEED) + (ANY_INPUT(player, GI_LEFT) ? 18432L : 24576L), FxZero);
	}

	if (ANY_INPUT(player, GI_JUMP) && VAL(actor, Y_SPEED) < FxZero && foot_y < game_state.water)
		VAL(actor, Y_SPEED) -= (player->power == POW_LUI)
		                               ? 39322L
		                               : ((Fabs(VAL(actor, X_SPEED)) < 40960L) ? 26214L : FxHalf);

	if (jumped) {
		VAL(actor, PLAYER_SPRING) = 7L;
		if (foot_y >= game_state.water) {
			VAL(actor, Y_TOUCH) = 0L;
			VAL(actor, Y_SPEED) = ((foot_y + actor->box.start.y) > (game_state.water + FxFrom(16L))
						      || ((foot_y + actor->box.end.y - FxOne) < game_state.water))
			                              ? FxFrom(-3L)
			                              : FxFrom(-9L);
			VAL(actor, PLAYER_GROUND) = 0L;
			VAL(actor, PLAYER_FRAME) = FxZero;
			FLAG_OFF(actor, FLG_PLAYER_DUCK);
			play_state_sound("swim", PLAY_POS, 0L, A_ACTOR(actor));
		}
	}
	VAL_TICK(actor, PLAYER_SPRING);

	if (foot_y >= game_state.water) {
		if (!ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			if (Fabs(VAL(actor, X_SPEED)) >= FxHalf)
				VAL(actor, X_SPEED) -= (VAL(actor, X_SPEED) >= FxZero) ? FxHalf : -FxHalf;
			VAL(actor, PLAYER_FRAME) = FxZero;
			if (foot_y < (game_state.water + FxFrom(11L)))
				create_actor(ACT_WATER_SPLASH, actor->pos);
			FLAG_ON(actor, FLG_PLAYER_SWIM);
		}
	} else if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
		VAL(actor, PLAYER_FRAME) = FxZero;
		FLAG_OFF(actor, FLG_PLAYER_SWIM);
	}

	if (foot_y < game_state.water) {
		if ((jumped || (ANY_INPUT(player, GI_JUMP) && ANY_FLAG(actor, FLG_PLAYER_JUMP)))
			&& VAL(actor, PLAYER_GROUND) > 0L)
		{
			VAL(actor, PLAYER_GROUND) = 0L;
			VAL(actor, Y_SPEED) = FxFrom(-13L);
			FLAG_OFF(actor, FLG_PLAYER_JUMP | FLG_PLAYER_DUCK);
			play_state_sound("jump", PLAY_POS, 0L, A_ACTOR(actor));
		}
		if (!ANY_INPUT(player, GI_JUMP) && VAL(actor, PLAYER_GROUND) > 0L && ANY_FLAG(actor, FLG_PLAYER_JUMP))
			FLAG_OFF(actor, FLG_PLAYER_JUMP);
	}
	if (foot_y > game_state.water && (game_state.time % 5L) == 0L)
		if (rng(10L) == 5L)
			create_actor(
				ACT_BUBBLE, POS_ADD(actor, FxZero,
						    (player->power == POW_SMALL || ANY_FLAG(actor, FLG_PLAYER_DUCK))
							    ? FxFrom(-18L)
							    : FxFrom(-39L)));

	if (jumped && VAL(actor, PLAYER_GROUND) <= 0L && VAL(actor, Y_SPEED) > FxZero)
		FLAG_ON(actor, FLG_PLAYER_JUMP);

	actor->box.start.y
		= (player->power == POW_SMALL || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? FxFrom(-26L) : FxFrom(-52L);

	VAL_TICK(actor, PLAYER_GROUND);

	if (ANY_FLAG(actor, FLG_PLAYER_ASCEND)) {
		move_actor(actor, POS_SPEED(actor));
		if (VAL(actor, Y_SPEED) >= FxZero)
			FLAG_OFF(actor, FLG_PLAYER_ASCEND);
	} else {
		displace_actor(actor, FxFrom(10L), TRUE);
		if (VAL(actor, Y_TOUCH) > 0L)
			VAL(actor, PLAYER_GROUND) = GROUND_TIME;

		const GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll != NULL) {
			if (game_state.sequence.type != SEQ_WIN) {
				if ((actor->pos.x + actor->box.start.x) < game_state.bounds.start.x) {
					move_actor(actor,
						(FVec2){game_state.bounds.start.x - actor->box.start.x, actor->pos.y});
					VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED), FxZero);
					VAL(actor, X_TOUCH) = -1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
				if ((actor->pos.x + actor->box.end.x) > game_state.bounds.end.x) {
					move_actor(actor,
						(FVec2){(game_state.bounds.end.x - actor->box.end.x), actor->pos.y});
					VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED), FxZero);
					VAL(actor, X_TOUCH) = 1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
			}

			if (ANY_FLAG(autoscroll, FLG_SCROLL_TANKS)
				&& ((actor->pos.y + actor->box.end.y) >= (game_state.bounds.end.y - FxFrom(64L))
					|| VAL(actor, PLAYER_GROUND) <= 0L)
				&& !touching_solid(HITBOX_ADD(actor, VAL(autoscroll, X_SPEED), FxZero), SOL_SOLID))
				move_actor(actor, POS_ADD(actor, VAL(autoscroll, X_SPEED), FxZero));
		}
	}

	GameActor* wave = get_actor(game_state.wave);
	if (wave != NULL && VAL(actor, PLAYER_GROUND) <= 0L
		&& !touching_solid(HITBOX_ADD(actor, FxZero, -VAL(wave, WAVE_DELTA)), SOL_ALL))
		move_actor(actor, POS_ADD(actor, FxZero, -VAL(wave, WAVE_DELTA)));

	foot_y = actor->pos.y - FxOne;
	if (VAL(actor, Y_TOUCH) <= 0L) {
		if (foot_y >= game_state.water) {
			if (VAL(actor, Y_SPEED) > FxFrom(3L))
				VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) - FxOne, FxFrom(3L));
			else if (VAL(actor, Y_SPEED) < FxFrom(3L))
				VAL(actor, Y_SPEED) += 6554L;
		} else if (VAL(actor, Y_SPEED) > FxFrom(10L)) {
			VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) - FxOne, FxFrom(10L));
		} else if (VAL(actor, Y_SPEED) < FxFrom(10L))
			VAL(actor, Y_SPEED) += FxOne;
	}

	// Animation
	if (VAL(actor, X_SPEED) > FxZero && (ANY_INPUT(player, GI_RIGHT) || game_state.sequence.type == SEQ_WIN))
		FLAG_OFF(actor, FLG_X_FLIP);
	else if (VAL(actor, X_SPEED) < FxZero && (ANY_INPUT(player, GI_LEFT) || game_state.sequence.type == SEQ_WIN))
		FLAG_ON(actor, FLG_X_FLIP);

	VAL_TICK(actor, PLAYER_FIRE);

	if (VAL(actor, PLAYER_GROUND) > 0L)
		if (VAL(actor, X_SPEED) == FxZero)
			VAL(actor, PLAYER_FRAME) = FxZero;
		else
			VAL(actor, PLAYER_FRAME) += Fclamp(Fmul(Fabs(VAL(actor, X_SPEED)), 5243L), 9175L, FxHalf);
	else if (ANY_FLAG(actor, FLG_PLAYER_SWIM))
		VAL(actor, PLAYER_FRAME) += Fclamp(Fmul(Fabs(VAL(actor, X_SPEED)), 5243L), 9175L, 16056L);
	else
		VAL(actor, PLAYER_FRAME) = FxZero;

	switch (player->power) {
	default:
		break;

	case POW_FIRE:
	case POW_BEETROOT:
	case POW_HAMMER: {
		if (!ANY_PRESSED(player, GI_FIRE) || ANY_FLAG(actor, FLG_PLAYER_DUCK))
			break;

		GameActorType mtype = ACT_NULL;
		ActorID midx = 0L;

		for (; midx < MAX_MISSILES; midx++)
			if (get_actor(player->missiles[midx]) == NULL)
				goto new_missile;
		break;

	new_missile:
		switch (player->power) {
		default:
			mtype = ACT_NULL;
			break;

		case POW_FIRE:
			mtype = ACT_MISSILE_FIREBALL;
			break;

		case POW_BEETROOT: {
			ActorID beetroots = 0L;
			for (ActorID i = 0L; i < MAX_SINK; i++)
				if (get_actor(player->sink[i]) != NULL)
					++beetroots;
			mtype = (beetroots >= MAX_SINK) ? ACT_NULL : ACT_MISSILE_BEETROOT;
			break;
		}

		case POW_HAMMER:
			mtype = ACT_MISSILE_HAMMER;
			break;
		}

		if (mtype == ACT_NULL)
			break;
		GameActor* missile = create_actor(
			mtype, POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FxFrom(-5L) : FxFrom(5L), FxFrom(-30L)));
		if (missile == NULL)
			break;

		player->missiles[midx] = missile->id;
		VAL(missile, MISSILE_PLAYER) = (ActorValue)player->id;
		switch (missile->type) {
		default:
			break;

		case ACT_MISSILE_FIREBALL: {
			FLAG_ON(missile, actor->flags & FLG_X_FLIP);
			VAL(missile, X_SPEED) = ANY_FLAG(missile, FLG_X_FLIP) ? -532480L : 532480L;
			break;
		}

		case ACT_MISSILE_BEETROOT: {
			VAL(missile, X_SPEED) = ANY_FLAG(actor, FLG_X_FLIP) ? -139264L : 139264L;
			VAL(missile, Y_SPEED) = FxFrom(-5L);
			break;
		}

		case ACT_MISSILE_HAMMER: {
			FLAG_ON(missile, actor->flags & FLG_X_FLIP);
			VAL(missile, X_SPEED)
				= (ANY_FLAG(actor, FLG_X_FLIP) ? -173015L : 173015L) + VAL(actor, X_SPEED);
			VAL(missile, Y_SPEED) = FxFrom(-7L) + Fmin(0L, Fhalf(VAL(actor, Y_SPEED)));
			break;
		}
		}

		if (VAL(actor, PLAYER_GROUND) > 0L)
			VAL(actor, PLAYER_FIRE) = 2L;
		play_state_sound("fire", PLAY_POS, 0L, A_ACTOR(missile));
		break;
	}

	case POW_LUI: {
		if (VAL(actor, PLAYER_GROUND) > 0L || (game_state.time % 2L) == 0L)
			break;

		GameActor* effect = create_actor(ACT_PLAYER_EFFECT, actor->pos);
		if (effect == NULL)
			break;
		FLAG_ON(effect, actor->flags & FLG_X_FLIP);
		VAL(effect, PLAYER_EFFECT_POWER) = player->power;
		VAL(effect, PLAYER_EFFECT_FRAME) = get_player_frame(actor);
		align_interp(effect, actor);

		break;
	}
	}

	VAL_TICK(actor, PLAYER_FLASH);

	if (VAL(actor, PLAYER_STARMAN) > 0L) {
		--VAL(actor, PLAYER_STARMAN);
		if (VAL(actor, PLAYER_STARMAN) == 100L)
			play_state_sound("starman", PLAY_POS, 0L, A_ACTOR(actor));
		if (VAL(actor, PLAYER_STARMAN) <= 0L) {
			VAL(actor, PLAYER_STARMAN_COMBO) = 0L;

			// !!! CLIENT-SIDE !!!
			if (VAL(actor, PLAYER_INDEX) == localplayer())
				stop_state_track(TS_POWER);
			// !!! CLIENT-SIDE !!!
		}
	}

	if (foot_y > (((get_actor(game_state.autoscroll) != NULL) ? game_state.bounds.end.y : player->bounds.end.y)
		      + ((wave != NULL) ? FxFrom(48L) : FxFrom(64L))))
	{
		kill_player(actor);
		goto sync_pos;
	}

skip_physics:
	if (VAL(actor, PLAYER_POWER) > 0L)
		VAL(actor, PLAYER_POWER) -= 91L;

	if ((game_state.flags & GF_KEVIN) && !(game_state.flags & GF_HUB)
		&& (game_state.sequence.type == SEQ_NONE || game_state.sequence.type == SEQ_AMBUSH
			|| game_state.sequence.type == SEQ_AMBUSH_END || game_state.sequence.type == SEQ_BOWSER_END)
		&& ((player->kevin.delay <= 0L && Vdist(actor->pos, player->kevin.start) > FxOne)
			|| player->kevin.delay > 0L))
	{
		for (Uint32 i = 0L; i < (KEVIN_DELAY - 1L); i++) {
			const Uint32 j = i + 1L;
			player->kevin.frames[i].pos = player->kevin.frames[j].pos;
			player->kevin.frames[i].power = player->kevin.frames[j].power;
			player->kevin.frames[i].frame = player->kevin.frames[j].frame;
			player->kevin.frames[i].flip = player->kevin.frames[j].flip;
		}

		if (player->kevin.delay < KEVIN_DELAY)
			if (++(player->kevin.delay) >= KEVIN_DELAY) {
				GameActor* kevin = create_actor(ACT_KEVIN, player->kevin.frames[0].pos);
				if (kevin != NULL) {
					player->kevin.actor = kevin->id;
					VAL(kevin, KEVIN_PLAYER) = (ActorValue)player->id;
					play_state_sound("kevin_spawn", PLAY_POS, 0L, A_ACTOR(kevin));
				}
			}

		player->kevin.frames[KEVIN_DELAY - 1L].pos = actor->pos;
		player->kevin.frames[KEVIN_DELAY - 1L].flip = ANY_FLAG(actor, FLG_X_FLIP);
		player->kevin.frames[KEVIN_DELAY - 1L].power = player->power;
		player->kevin.frames[KEVIN_DELAY - 1L].frame = get_player_frame(actor);
	}

	collide_actor(actor);
	FLAG_OFF(actor, FLG_PLAYER_STOMP);

sync_pos:
	player->pos = actor->pos;
}

static void draw(const GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL || (VAL(actor, PLAYER_FLASH) > 0L && (game_state.time % 2L)))
		return;

	const char* tex = get_player_texture(player->power, get_player_frame(actor));
	// !!! CLIENT-SIDE !!!
	const float a = (localplayer() == player->id) ? 255L : 191L;
	// !!! CLIENT-SIDE !!!
	draw_actor_no_jitter(actor, tex, 0.f, B_ALPHA(a));

	if (VAL(actor, PLAYER_STARMAN) > 0L) {
		float r = 0.9725490196078431f, g = 0.f, b = 0.f;
		switch (game_state.time % 5L) {
		default:
			break;
		case 1L: {
			r = 0.7529411764705882f;
			g = 0.596078431372549f;
			b = 0.f;
			break;
		}
		case 2L: {
			r = 0.5647058823529412f;
			g = 0.7529411764705882f;
			b = 0.1568627450980392f;
			break;
		}
		case 3L: {
			r = 0.3450980392156863f;
			g = 0.5333333333333333f;
			b = 0.9098039215686275f;
			break;
		}
		case 4L: {
			r = 0.7529411764705882f;
			g = 0.4705882352941176f;
			b = 0.7843137254901961f;
			break;
		}
		}

		batch_stencil(B_STENCIL(r, g, b, 1.f));
		batch_blend(B_BLEND(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE));
		draw_actor_no_jitter(actor, tex, 0.f, B_ALPHA(a));
		batch_blend(B_BLEND_NORMAL), batch_stencil(B_NO_STENCIL);
	}

	// !!! CLIENT-SIDE !!!
	if (viewplayer() == player->id)
		return;
	const char* name = get_player_name(player->id);
	if (name == NULL)
		return;
	const InterpActor* iactor = get_interp(actor);
	batch_pos(B_XYZ(Fx2Int(iactor->pos.x), Fx2Int(iactor->pos.y) + Fx2Float(actor->box.start.y) - 10.f, -1000.f));
	batch_color(B_ALPHA(160L));
	batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
	batch_string("main", 20, name);
	// !!! CLIENT-SIDE !!!
}

static void cleanup(GameActor* actor) {
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
	if (player == NULL)
		return;
	if (player->actor == actor->id)
		player->actor = NULLACT;
}

static void collide(GameActor* actor, GameActor* from) {
	if (get_actor((ActorID)VAL(actor, PLAYER_WARP)) != NULL)
		return;

	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, FxFrom(-16L), 0))
			VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
		else if (Fabs(VAL(from, X_SPEED)) > Fabs(VAL(actor, X_SPEED)))
			if ((VAL(from, X_SPEED) > FxZero && from->pos.x < actor->pos.x)
				|| (VAL(from, X_SPEED) < FxZero && from->pos.x > actor->pos.x))
			{
				VAL(actor, X_SPEED) += Fhalf(VAL(from, X_SPEED));
				VAL(from, X_SPEED) = -Fhalf(VAL(from, X_SPEED));
				if (Fabs(VAL(from, X_SPEED)) > FxFrom(2L))
					play_state_sound("bump", PLAY_POS, 0L, A_ACTOR(actor));
			}

		break;
	}

	case ACT_BLOCK_BUMP: {
		if (get_owner_id(actor) != get_owner_id(from) && VAL(actor, PLAYER_GROUND) > 0L) {
			VAL(actor, Y_SPEED) = FxFrom(-8L);
			VAL(actor, PLAYER_GROUND) = 0L;
		}
		break;
	}
	}
}

static PlayerID owner(const GameActor* actor) {
	return (PlayerID)VAL(actor, PLAYER_INDEX);
}

const GameActorTable TAB_PLAYER = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.cleanup = cleanup,
	.collide = collide,
	.owner = owner,
};

// ===========
// DEAD PLAYER
// ===========

static void load_corpse() {
	load_texture("player/mario/dead", FALSE);

	load_sound("lose", FALSE);
	load_sound("dead", FALSE);
	load_sound("hardcore", FALSE);

	load_track("lose", FALSE);
	load_track("lose2", FALSE);
	load_track("game_over", FALSE);
}

static void create_corpse(GameActor* actor) {
	actor->depth = -FxOne;
	VAL(actor, PLAYER_INDEX) = (ActorValue)NULLPLAY;
}

static void tick_corpse(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL && ANY_FLAG(autoscroll, FLG_SCROLL_TANKS))
		move_actor(actor, POS_ADD(actor, VAL(autoscroll, X_SPEED), VAL(autoscroll, Y_SPEED)));

	GameActor* wave = get_actor(game_state.wave);
	if (wave != NULL)
		move_actor(actor, POS_ADD(actor, FxZero, -VAL(wave, WAVE_DELTA)));

	if (VAL(actor, PLAYER_FRAME) >= 25L) {
		VAL(actor, Y_SPEED) += 26214L;
		move_actor(actor, POS_SPEED(actor));
	}

	switch (VAL(actor, PLAYER_FRAME)++) {
	default:
		break;

	case 0L: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD)) {
			GameActor* autoscroll = get_actor(game_state.autoscroll);
			if (autoscroll != NULL && !ANY_FLAG(autoscroll, FLG_SCROLL_TANKS | FLG_SCROLL_BOWSER))
				VAL(autoscroll, X_SPEED) = VAL(autoscroll, Y_SPEED) = FxZero;

			if (game_state.flags & GF_HARDCORE)
				play_state_sound("hardcore", 0L, 0L, A_NULL);

			for (TrackSlots i = 0; i < (TrackSlots)TS_SIZE; i++)
				stop_state_track(i);
			play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? "lose2" : "lose", 0L, 0L);
		} else
			play_state_sound((player->lives >= 0L || (game_state.flags & GF_HELL)) ? "lose" : "dead",
				PLAY_POS, 0L, A_ACTOR(actor));
		break;
	}

	case 25L:
		VAL(actor, Y_SPEED) = FxFrom(-10L);
		break;

	case 150L: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD) || (game_state.flags & GF_SINGLE))
			break;

		GameActor* pawn = respawn_player(player);
		if (pawn != NULL) {
			play_state_sound("respawn", PLAY_POS, 0L, A_ACTOR(pawn));
			VAL(pawn, PLAYER_FLASH) = 100L;
			FLAG_ON(actor, FLG_DESTROY);
		}

		break;
	}

	case 200L: {
		if (game_state.sequence.type == SEQ_LOSE)
			for (PlayerID i = 0L; i < numplayers(); i++) {
				GamePlayer* p = get_player(i);
				if (p != NULL && (p->lives >= 0L || (game_state.flags & GF_HELL))) {
					game_state.flags |= GF_RESTART;
					break;
				}
			}

		break;
	}

	case 210L: {
		if ((ANY_FLAG(actor, FLG_PLAYER_DEAD) || game_state.clock == 0L) && game_state.sequence.type != SEQ_WIN)
		{
			game_state.sequence.type = SEQ_LOSE;
			game_state.sequence.time = 1L;
			play_state_track(TS_FANFARE, "game_over", 0L, 0L);
		}

		FLAG_ON(actor, FLG_DESTROY);
		break;
	}
	}
}

static void draw_corpse(const GameActor* actor) {
	draw_actor_no_jitter(
		actor, "player/mario/dead", 0.f, B_ALPHA((localplayer() == get_owner_id(actor)) ? 255L : 191L));

	if (!ANY_FLAG(actor, FLG_PLAYER_JACKASS))
		return;
	const InterpActor* iactor = get_interp(actor);
	batch_reset();
	batch_pos(B_XYZ(Fx2Int(iactor->pos.x), Fx2Int(iactor->pos.y + actor->box.start.y) - 32.f, -1000.f));
	batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM)), batch_string("main", 24, "JACKASS");
}

const GameActorTable TAB_PLAYER_DEAD = {
	.load = load_corpse,
	.create = create_corpse,
	.tick = tick_corpse,
	.draw = draw_corpse,
	.owner = owner,
};

// =============
// PLAYER EFFECT
// =============

static void create_effect(GameActor* actor) {
	actor->box.start.x = FxFrom(-24L);
	actor->box.start.y = FxFrom(-64L);
	actor->box.end.x = FxFrom(24L);

	VAL(actor, PLAYER_EFFECT_INDEX) = (ActorValue)NULLPLAY;
	VAL(actor, PLAYER_EFFECT_POWER) = POW_SMALL;
	VAL(actor, PLAYER_EFFECT_FRAME) = PF_IDLE;
	VAL(actor, PLAYER_EFFECT_ALPHA) = FxFrom(255L);
}

static void tick_effect(GameActor* actor) {
	actor->depth += 4095L;

	VAL(actor, PLAYER_EFFECT_ALPHA) -= 652800L;
	if (VAL(actor, PLAYER_EFFECT_ALPHA) <= FxZero)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_effect(const GameActor* actor) {
	draw_actor(actor, get_player_texture(VAL(actor, PLAYER_EFFECT_POWER), VAL(actor, PLAYER_EFFECT_FRAME)), 0.f,
		B_ALPHA(((localplayer() == get_owner_id(actor)) ? 1.f : 0.75f)
			* Fx2Int(VAL(actor, PLAYER_EFFECT_ALPHA))));
}

static PlayerID effect_owner(const GameActor* actor) {
	return (PlayerID)VAL(actor, PLAYER_EFFECT_INDEX);
}

const GameActorTable TAB_PLAYER_EFFECT = {
	.load = load_player_textures,
	.create = create_effect,
	.tick = tick_effect,
	.draw = draw_effect,
	.owner = effect_owner,
};

// =====
// KEVIN
// =====

static void load_kevin() {
	load_player_textures();

	load_sound("kevin_spawn", FALSE);
	load_sound("kevin_kill", FALSE);

	load_actor(ACT_EXPLODE);
}

static void create_kevin(GameActor* actor) {
	actor->depth = -FxOne;

	actor->box.start.x = FxFrom(-9L);
	actor->box.start.y = FxFrom(-26L);
	actor->box.end.x = FxFrom(10L);

	VAL(actor, KEVIN_PLAYER) = (ActorValue)NULLPLAY;
}

static void tick_kevin(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	move_actor(actor, player->kevin.frames[0].pos);
	if (player->kevin.frames[0].flip)
		FLAG_ON(actor, FLG_X_FLIP);
	else
		FLAG_OFF(actor, FLG_X_FLIP);
}

static void draw_kevin(const GameActor* actor) {
	const GamePlayer* player = get_owner(actor);
	if (player == NULL)
		return;

	const InterpActor* iactor = get_interp(actor);
	float pos[3] = {Fx2Int(iactor->pos.x), Fx2Int(iactor->pos.y), Fx2Float(actor->depth)};
	Uint8 color[4] = {80, 80, 80, (localplayer() == player->id) ? 255 : 191};
	const Bool flip[2] = {ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)};
	const char* tex = get_player_texture(player->kevin.frames[0].power, player->kevin.frames[0].frame);

	batch_reset(), batch_pos(pos), batch_color(color), batch_flip(flip), batch_sprite(tex);
	// THIS USES SDL_rand(), NOT rng() THAT ONE IS USED FOR THE GAME STATE
	pos[0] += (float)(-5L + SDL_rand(11L)), pos[1] += (float)(-5L + SDL_rand(11L));
	color[3] = (Uint8)((float)color[3] * 0.75f);
	batch_pos(pos), batch_color(color), batch_sprite(tex);
}

static void collide_kevin(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || get_actor(VAL(from, PLAYER_WARP)) != NULL)
		return;
	if (get_owner(actor) == get_owner(from)) {
		kill_player(from);
		play_state_sound("kevin_kill", PLAY_POS, 0L, A_ACTOR(actor));
	} else
		hit_player(from);
}

static PlayerID kevin_owner(const GameActor* actor) {
	return VAL(actor, KEVIN_PLAYER);
}

const GameActorTable TAB_KEVIN = {
	.load = load_kevin,
	.create = create_kevin,
	.tick = tick_kevin,
	.draw = draw_kevin,
	.collide = collide_kevin,
	.owner = kevin_owner,
};

// ====
// FRED
// ====

static void load_fred() {
	load_texture("enemies/fred", FALSE);

	load_actor(ACT_EXPLODE);

	if (video_state.fred_surface == NULL)
		video_state.fred_surface = create_surface(86L, 86L, TRUE, FALSE);
}

static void create_fred(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FxFrom(-30L);
	actor->box.end.x = actor->box.end.y = FxFrom(30L);
}

static void tick_fred(GameActor* actor) {
	if (VAL(actor, FRED_ALPHA) < FxFrom(255L)) {
		VAL(actor, FRED_ALPHA) += 278528L;
		if (VAL(actor, FRED_ALPHA) >= FxFrom(255L))
			VAL(actor, FRED_ALPHA) = FxFrom(255L);
		else
			return;
	}

	if (game_state.sequence.type == SEQ_WIN || game_state.sequence.type == SEQ_LOSE) {
		create_actor(ACT_EXPLODE, actor->pos);
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, X_SPEED) = Fmul(VAL(actor, X_SPEED), 64881L);
	VAL(actor, Y_SPEED) = Fmul(VAL(actor, Y_SPEED), 64881L);

	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		if (!in_any_view(actor, FxZero, FALSE)) {
			GamePlayer* player = get_owner(nearest);
			if (player != NULL && !Rcollide(HITBOX(actor), player->bounds)
				&& get_actor(VAL(nearest, PLAYER_WARP)) == NULL)
			{
				VAL(actor, X_SPEED) = VAL(actor, Y_SPEED) = VAL(actor, FRED_ALPHA) = FxZero;
				move_actor(actor, nearest->pos);
				skip_interp(actor);
			}
		}
		if (get_actor(VAL(nearest, PLAYER_WARP)) == NULL) {
			const Fixed dir = Vtheta(actor->pos, POS_ADD(nearest, FxZero, FxFrom(-16L)));
			VAL(actor, X_SPEED) += Fmul(Fcos(dir), 10000L);
			VAL(actor, Y_SPEED) += Fmul(Fsin(dir), 10000L);
		}
	} else
		VAL(actor, X_SPEED) += (VAL(actor, X_SPEED) >= FxZero) ? 10000L : -10000L;

	move_actor(actor, POS_SPEED(actor));
}

static void draw_fred(const GameActor* actor) {
	push_surface(video_state.fred_surface);
	clear_color(0.f, 0.f, 0.f, 0.f);

	mat4 view = GLM_MAT4_IDENTITY_INIT, proj = GLM_MAT4_IDENTITY_INIT;
	glm_lookat(B_XYZ(-198.f, 0.f, -98.f), B_ORIGIN, GLM_ZUP, view);
	glm_perspective(0.78539816339f, 1.f, 0.f, 1000.f, proj);
	set_view_matrix(view), set_projection_matrix(proj), apply_matrices();

	batch_reset();
	batch_angle(totalticks() * 0.03590391604f), batch_sprite("enemies/fred");

	pop_surface();

	const InterpActor* iactor = get_interp(actor);
	const float ax = Fx2Int(iactor->pos.x), ay = Fx2Int(iactor->pos.y), az = Fx2Float(actor->depth);
	Uint8 col[4] = {255L, 255L, 255L, Fx2Int(VAL(actor, FRED_ALPHA))};

	batch_reset();
	batch_pos(B_XYZ(ax, ay, az)), batch_offset(B_XY(43.f, 43.f)), batch_color(col);
	batch_surface(video_state.fred_surface);

	col[0] = col[1] = col[2] = 0L, batch_color(col);
	batch_rectangle(NULL, B_XY(86.f, 1.f));
	batch_rectangle(NULL, B_XY(1.f, 86.f));
	batch_pos(B_XYZ(ax, ay + 85.f, az)), batch_rectangle(NULL, B_XY(86.f, 1.f));
	batch_pos(B_XYZ(ax + 85.f, ay, az)), batch_rectangle(NULL, B_XY(1.f, 86.f));
}

static void collide_fred(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER && VAL(actor, FRED_ALPHA) >= FxFrom(255L)
		&& get_actor(VAL(from, PLAYER_WARP)) == NULL)
		kill_player(from);
}

const GameActorTable TAB_FRED = {
	.load = load_fred,
	.create = create_fred,
	.tick = tick_fred,
	.draw = draw_fred,
	.collide = collide_fred,
};
