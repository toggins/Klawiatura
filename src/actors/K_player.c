#include "actors/K_autoscroll.h"
#include "actors/K_enemies.h"
#include "actors/K_missiles.h" // IWYU pragma: keep
#include "actors/K_platform.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

#define GROUND_TIME 5L

// ================
// HELPER FUNCTIONS
// ================

static void load_player_textures() {
	load_texture("player/mario/dead");
	load_texture("player/mario/grow");
	load_texture("player/mario/grow2");
	load_texture("player/mario/grow3");

	load_texture("player/mario/small/idle");
	load_texture("player/mario/small/walk");
	load_texture("player/mario/small/walk2");
	load_texture("player/mario/small/jump");
	load_texture("player/mario/small/swim");
	load_texture("player/mario/small/swim2");
	load_texture("player/mario/small/swim3");
	load_texture("player/mario/small/swim4");

	load_texture("player/mario/big/idle");
	load_texture("player/mario/big/walk");
	load_texture("player/mario/big/walk2");
	load_texture("player/mario/big/jump");
	load_texture("player/mario/big/duck");
	load_texture("player/mario/big/swim");
	load_texture("player/mario/big/swim2");
	load_texture("player/mario/big/swim3");
	load_texture("player/mario/big/swim4");

	load_texture("player/mario/fire/idle");
	load_texture("player/mario/fire/walk");
	load_texture("player/mario/fire/walk2");
	load_texture("player/mario/fire/jump");
	load_texture("player/mario/fire/duck");
	load_texture("player/mario/fire/fire");
	load_texture("player/mario/fire/swim");
	load_texture("player/mario/fire/swim2");
	load_texture("player/mario/fire/swim3");
	load_texture("player/mario/fire/swim4");

	load_texture("player/mario/beetroot/idle");
	load_texture("player/mario/beetroot/walk");
	load_texture("player/mario/beetroot/walk2");
	load_texture("player/mario/beetroot/jump");
	load_texture("player/mario/beetroot/duck");
	load_texture("player/mario/beetroot/fire");
	load_texture("player/mario/beetroot/swim");
	load_texture("player/mario/beetroot/swim2");
	load_texture("player/mario/beetroot/swim3");
	load_texture("player/mario/beetroot/swim4");

	load_texture("player/mario/lui/idle");
	load_texture("player/mario/lui/walk");
	load_texture("player/mario/lui/walk2");
	load_texture("player/mario/lui/jump");
	load_texture("player/mario/lui/duck");
	load_texture("player/mario/lui/swim");
	load_texture("player/mario/lui/swim2");
	load_texture("player/mario/lui/swim3");
	load_texture("player/mario/lui/swim4");

	load_texture("player/mario/hammer/idle");
	load_texture("player/mario/hammer/walk");
	load_texture("player/mario/hammer/walk2");
	load_texture("player/mario/hammer/jump");
	load_texture("player/mario/hammer/duck");
	load_texture("player/mario/hammer/fire");
	load_texture("player/mario/hammer/swim");
	load_texture("player/mario/hammer/swim2");
	load_texture("player/mario/hammer/swim3");
	load_texture("player/mario/hammer/swim4");
}

PlayerFrame get_player_frame(const GameActor* actor) {
	//     const GameActor* warp = get_actor((ActorID)VAL(actor,VAL_PLAYER_WARP));
	//     if (warp != NULL)
	//         switch (VAL(warp,VAL_WARP_ANGLE)) {
	//             case 0:
	//             case 2:
	//                 switch (FtInt(VAL(actor,VAL_PLAYER_FRAME)) % 3L) {
	//                     default:
	//                         return PF_WALK1;
	//                     case 1:
	//                         return PF_WALK2;
	//                     case 2:
	//                         return PF_WALK3;
	//                 }
	//             case 1:
	//                 return PF_JUMP;
	//             default:
	//                 return PF_DUCK;
	//         }

	if (VAL(actor, PLAYER_POWER) > 0L) {
		const GamePlayer* player = get_player((PlayerID)VAL(actor, PLAYER_INDEX));
		switch ((player == NULL) ? POW_SMALL : player->power) {
		case POW_SMALL:
		case POW_BIG: {
			switch ((VAL(actor, PLAYER_POWER) / 100L) % 3L) {
			default:
				return PF_GROW1;
			case 1:
				return PF_GROW2;
			case 2:
				return PF_GROW3;
			}
			break;
		}

		default: {
			switch ((VAL(actor, PLAYER_POWER) / 100L) % 4L) {
			default:
				return PF_GROW1;
			case 1:
				return PF_GROW2;
			case 2:
				return PF_GROW3;
			case 3:
				return PF_GROW4;
			}
			break;
		}
		}
	}

	if (VAL(actor, PLAYER_GROUND) <= 0L) {
		if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			const int32_t frame = FtInt(VAL(actor, PLAYER_FRAME));
			switch (frame) {
			case 0:
			case 3:
				return PF_SWIM1;
			case 1:
			case 4:
				return PF_SWIM2;
			case 2:
			case 5:
				return PF_SWIM3;
			default:
				return (frame % 2L) ? PF_SWIM5 : PF_SWIM4;
			}
		} else
			return (VAL(actor, Y_SPEED) < FxZero) ? PF_JUMP : PF_FALL;
	}

	if (ANY_FLAG(actor, FLG_PLAYER_DUCK))
		return PF_DUCK;

	if (VAL(actor, PLAYER_FIRE) > 0L)
		return PF_FIRE;

	if (VAL(actor, X_SPEED) != FxZero)
		switch (FtInt(VAL(actor, PLAYER_FRAME)) % 3L) {
		default:
			return PF_WALK1;
		case 1:
			return PF_WALK2;
		case 2:
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
			return "player/mario/small/walk";
		case PF_WALK2:
			return "player/mario/small/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/small/jump";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/small/swim";
		case PF_SWIM2:
			return "player/mario/small/swim2";
		case PF_SWIM3:
			return "player/mario/small/swim3";
		case PF_SWIM5:
			return "player/mario/small/swim4";

		case PF_GROW2:
			return "player/mario/grow";
		case PF_GROW3:
			return "player/mario/big/walk2";
		}
		break;
	}

	case POW_BIG: {
		switch (frame) {
		default:
			return "player/mario/big/idle";

		case PF_WALK1:
			return "player/mario/big/walk";
		case PF_WALK2:
		case PF_GROW1:
			return "player/mario/big/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/big/jump";

		case PF_DUCK:
			return "player/mario/big/duck";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/big/swim";
		case PF_SWIM2:
			return "player/mario/big/swim2";
		case PF_SWIM3:
			return "player/mario/big/swim3";
		case PF_SWIM5:
			return "player/mario/big/swim4";

		case PF_GROW2:
			return "player/mario/grow";
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
			return "player/mario/fire/walk";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/fire/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/fire/jump";

		case PF_DUCK:
			return "player/mario/fire/duck";

		case PF_FIRE:
			return "player/mario/fire/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/fire/swim";
		case PF_SWIM2:
			return "player/mario/fire/swim2";
		case PF_SWIM3:
			return "player/mario/fire/swim3";
		case PF_SWIM5:
			return "player/mario/fire/swim4";

		case PF_GROW1:
			return "player/mario/big/walk2";
		case PF_GROW2:
			return "player/mario/grow2";
		case PF_GROW4:
			return "player/mario/grow3";
		}
		break;
	}

	case POW_BEETROOT: {
		switch (frame) {
		default:
			return "player/mario/beetroot/idle";

		case PF_WALK1:
			return "player/mario/beetroot/walk";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/beetroot/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/beetroot/jump";

		case PF_DUCK:
			return "player/mario/beetroot/duck";

		case PF_FIRE:
			return "player/mario/beetroot/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/beetroot/swim";
		case PF_SWIM2:
			return "player/mario/beetroot/swim2";
		case PF_SWIM3:
			return "player/mario/beetroot/swim3";
		case PF_SWIM5:
			return "player/mario/beetroot/swim4";

		case PF_GROW1:
			return "player/mario/big/walk2";
		case PF_GROW2:
			return "player/mario/grow2";
		case PF_GROW4:
			return "player/mario/grow3";
		}
		break;
	}

	case POW_LUI: {
		switch (frame) {
		default:
			return "player/mario/lui/idle";

		case PF_WALK1:
			return "player/mario/lui/walk";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/lui/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/lui/jump";

		case PF_DUCK:
			return "player/mario/lui/duck";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/lui/swim";
		case PF_SWIM2:
			return "player/mario/lui/swim2";
		case PF_SWIM3:
			return "player/mario/lui/swim3";
		case PF_SWIM5:
			return "player/mario/lui/swim4";

		case PF_GROW1:
			return "player/mario/big/walk2";
		case PF_GROW2:
			return "player/mario/grow2";
		case PF_GROW4:
			return "player/mario/grow3";
		}
		break;
	}

	case POW_HAMMER: {
		switch (frame) {
		default:
			return "player/mario/hammer/idle";

		case PF_WALK1:
			return "player/mario/hammer/walk";
		case PF_WALK2:
		case PF_GROW3:
			return "player/mario/hammer/walk2";

		case PF_JUMP:
		case PF_FALL:
			return "player/mario/hammer/jump";

		case PF_DUCK:
			return "player/mario/hammer/duck";

		case PF_FIRE:
			return "player/mario/hammer/fire";

		case PF_SWIM1:
		case PF_SWIM4:
			return "player/mario/hammer/swim";
		case PF_SWIM2:
			return "player/mario/hammer/swim2";
		case PF_SWIM3:
			return "player/mario/hammer/swim3";
		case PF_SWIM5:
			return "player/mario/hammer/swim4";

		case PF_GROW1:
			return "player/mario/big/walk2";
		case PF_GROW2:
			return "player/mario/grow2";
		case PF_GROW4:
			return "player/mario/grow3";
		}
		break;
	}
	}
}

Bool hit_player(GameActor* actor) {
	if (actor == NULL || actor->type != ACT_PLAYER || VAL(actor, PLAYER_FLASH) > 0L
		|| VAL(actor, PLAYER_STARMAN) > 0L || VAL(actor, PLAYER_WARP) > 0L
		|| game_state.sequence.type == SEQ_WIN)
		return false;

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		if (player->power == POW_SMALL) {
			kill_player(actor);
			return true;
		}
		player->power = (player->power == POW_BIG) ? POW_SMALL : POW_BIG;
	}

	VAL(actor, PLAYER_POWER) = 3000L, VAL(actor, PLAYER_FLASH) = 100L;
	play_actor_sound(actor, "warp");
	return true;
}

void kill_player(GameActor* actor) {
	if (actor->type != ACT_PLAYER)
		return;

	GameActor* dead = create_actor(ACT_PLAYER_DEAD, actor->pos);
	if (dead == NULL)
		return;
	align_interp(dead, actor);

	// !!! CLIENT-SIDE !!!
	if (localplayer() == VAL(actor, PLAYER_INDEX) && VAL(actor, PLAYER_STARMAN) > 0L)
		stop_state_track(TS_POWER);
	// !!! CLIENT-SIDE !!!

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		VAL(dead, PLAYER_INDEX) = (ActorValue)player->id;

		--player->lives;
		player->power = POW_SMALL;

		GameActor* kpawn = get_actor(player->kevin.actor);
		if (kpawn != NULL) {
			align_interp(create_actor(ACT_EXPLODE, POS_ADD(kpawn, FxZero, FfInt(-16L))), kpawn);
			FLAG_ON(kpawn, FLG_DESTROY);
		}
		player->kevin.actor = NULLACT;
	}

	Bool all_dead = true;
	if (game_state.sequence.type == SEQ_NONE && game_state.clock != 0L && !(game_state.flags & GF_SINGLE))
		for (PlayerID i = 0; i < numplayers(); i++) {
			GamePlayer* survivor = get_player(i);
			if (survivor != NULL && survivor->lives >= 0L) {
				all_dead = false;
				break;
			}
		}
	if (all_dead) {
		FLAG_ON(dead, FLG_PLAYER_DEAD);
		if (!(game_state.flags & GF_SINGLE) && game_state.sequence.type == SEQ_WIN && rng(100L) == 0L)
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

	game_state.sequence.type = SEQ_WIN;
	game_state.sequence.time = 1L;
	game_state.sequence.activator = player->id;

	for (PlayerID i = 0L; i < numplayers(); i++) {
		GamePlayer* p = get_player(i);
		if (p == player)
			continue;

		GameActor* pawn = get_actor(p->actor);
		if (pawn != NULL)
			FLAG_ON(pawn, FLG_DESTROY);
		p->actor = NULLACT;
	}

	for (ActorID i = 0L; i < MAX_MISSILES; i++) {
		GameActor* missile = get_actor(player->missiles[i]);
		if (missile == NULL)
			continue;

		int32_t points;
		switch (missile->type) {
		default:
			points = 100L;
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
	}

	game_state.pswitch = 0L;
	VAL(actor, PLAYER_FLASH) = VAL(actor, PLAYER_STARMAN) = 0L;
	set_view_player(player);

	for (TrackSlots i = 0L; i < (TrackSlots)TS_SIZE; i++)
		stop_state_track(i);
	play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? "win2" : "win", false);
}

void player_starman(GameActor* actor, GameActor* from) {
	if (actor == NULL || from == NULL)
		return;

	int32_t points;
	switch (VAL(actor, PLAYER_STARMAN_COMBO)++) {
	case 0:
		points = 100L;
		break;
	case 1:
		points = 200L;
		break;
	case 2:
		points = 500L;
		break;
	case 3:
		points = 1000L;
		break;
	case 4:
		points = 2000L;
		break;
	case 5:
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
		(fvec2){Flerp(Fadd(from->pos.x, from->box.start.x), Fadd(from->pos.x, from->box.end.x), FxHalf),
			Flerp(Fadd(from->pos.y, from->box.start.y), Fadd(from->pos.y, from->box.end.y), FxHalf)});
	kill_enemy(from, true);
}

// ============
// PLAYER SPAWN
// ============

static void create_spawn(GameActor* actor) {
	game_state.spawn = actor->id;
}

static void cleanup_spawn(GameActor* actor) {
	if (game_state.spawn == actor->id)
		game_state.spawn = NULLACT;
}

const GameActorTable TAB_PLAYER_SPAWN = {.create = create_spawn, .cleanup = cleanup_spawn};

// ======
// PLAYER
// ======

static void load() {
	load_player_textures();

	load_sound("jump");
	load_sound("fire");
	load_sound("swim");
	load_sound("warp");
	load_sound("starman");
	load_sound("bump");
	load_sound("stomp");
	load_sound("respawn");

	load_track("win");
	load_track("win2");
	load_track("win3");

	load_actor(ACT_PLAYER_EFFECT);
	load_actor(ACT_PLAYER_DEAD);
	load_actor(ACT_MISSILE_FIREBALL);
	load_actor(ACT_MISSILE_BEETROOT);
	load_actor(ACT_MISSILE_BEETROOT_SINK);
	load_actor(ACT_MISSILE_HAMMER);
	load_actor(ACT_POINTS);
	load_actor(ACT_KEVIN);
}

static void create(GameActor* actor) {
	actor->depth = -FxOne;

	actor->box.start.x = FfInt(-9L);
	actor->box.start.y = FfInt(-25L);
	actor->box.end.x = FfInt(10L);
	actor->box.end.y = FxOne;

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
			move_actor(actor, (fvec2){Flerp(game_state.bounds.start.x, game_state.bounds.end.x, FxHalf),
						  actor->pos.y + FxOne});
			return;
		} else
			FLAG_OFF(actor, FLG_PLAYER_RESPAWN);
	}

	// TODO: Warp

	if (game_state.sequence.type == SEQ_WIN) {
		player->input &= GI_WARP;
		VAL(actor, X_SPEED) = 163840L;
	}

	const ActorID pfid = (ActorID)VAL(actor, PLAYER_PLATFORM);
	GameActor* platform = get_actor(pfid);
	if (platform != NULL) {
		const fixed vx = VAL(actor, X_SPEED), vy = VAL(actor, Y_SPEED),
			    pvx = platform->pos.x - VAL(platform, PLATFORM_X_FROM),
			    pvy = platform->pos.y - VAL(platform, PLATFORM_Y_FROM);

		VAL(actor, X_SPEED) = pvx, VAL(actor, Y_SPEED) = pvy;
		displace_actor(actor, FxZero, false);
		VAL(actor, X_SPEED) = vx, VAL(actor, Y_SPEED) = vy;

		frect mbox = HITBOX_ADD(actor, pvx, pvy);
		mbox.end.y += FxOne;
		const frect pbox = HITBOX(platform);
		VAL(actor, PLAYER_PLATFORM) = Rcollide(mbox, pbox) ? pfid : NULLACT;
	}

	const Bool cant_run = (!ANY_INPUT(player, GI_RUN) || actor->pos.y >= game_state.water),
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

	if (ANY_INPUT(player, GI_DOWN) && !ANY_INPUT(player, GI_LEFT | GI_RIGHT) && VAL(actor, PLAYER_GROUND) > 0L
		&& player->power != POW_SMALL)
		FLAG_ON(actor, FLG_PLAYER_DUCK);
	if (ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
		if (!ANY_INPUT(player, GI_DOWN) || player->power == POW_SMALL)
			FLAG_OFF(actor, FLG_PLAYER_DUCK);
		else if (VAL(actor, X_SPEED) > FxZero)
			VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - 24576L, FxZero);
		else if (VAL(actor, X_SPEED) < FxZero)
			VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + 24576L, FxZero);
	}

	if (ANY_INPUT(player, GI_JUMP) && VAL(actor, Y_SPEED) < FxZero && actor->pos.y < game_state.water
		&& !ANY_INPUT(player, GI_DOWN))
		VAL(actor, Y_SPEED) -= (player->power == POW_LUI)
		                               ? 39322L
		                               : ((Fabs(VAL(actor, X_SPEED)) < 40960L) ? 26214L : FxHalf);

	if (jumped) {
		VAL(actor, PLAYER_SPRING) = 7L;
		if (actor->pos.y >= game_state.water && !ANY_INPUT(player, GI_DOWN)) {
			VAL(actor, Y_TOUCH) = 0L;
			VAL(actor, Y_SPEED)
				= FfInt(((actor->pos.y + actor->box.start.y)
						> (game_state.water + FfInt(16L)
							|| (actor->pos.y + actor->box.end.y) < game_state.water))
						? -3L
						: -9L);
			VAL(actor, PLAYER_GROUND) = 0L;
			VAL(actor, PLAYER_FRAME) = FxZero;
			play_actor_sound(actor, "swim");
		}
	}
	VAL_TICK(actor, PLAYER_SPRING);

	if (actor->pos.y >= game_state.water) {
		if (!ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			if (Fabs(VAL(actor, X_SPEED)) >= FxHalf)
				VAL(actor, X_SPEED) -= (VAL(actor, X_SPEED) >= FxZero) ? FxHalf : -FxHalf;
			VAL(actor, PLAYER_FRAME) = FxZero;
			if (actor->pos.y < (game_state.water + FfInt(11L)))
				create_actor(ACT_WATER_SPLASH, actor->pos);
			FLAG_ON(actor, FLG_PLAYER_SWIM);
		}
	} else if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
		VAL(actor, PLAYER_FRAME) = FxZero;
		FLAG_OFF(actor, FLG_PLAYER_SWIM);
	}

	if (actor->pos.y < game_state.water) {
		if ((jumped || (ANY_INPUT(player, GI_JUMP) && ANY_FLAG(actor, FLG_PLAYER_JUMP)))
			&& !ANY_INPUT(player, GI_DOWN) && VAL(actor, PLAYER_GROUND) > 0L)
		{
			VAL(actor, PLAYER_GROUND) = 0L;
			VAL(actor, Y_SPEED) = FfInt(-13L);
			FLAG_OFF(actor, FLG_PLAYER_JUMP);
			play_actor_sound(actor, "jump");
		}
		if (!ANY_INPUT(player, GI_JUMP) && !ANY_INPUT(player, GI_DOWN) && VAL(actor, PLAYER_GROUND) > 0L
			&& ANY_FLAG(actor, FLG_PLAYER_JUMP))
			FLAG_OFF(actor, FLG_PLAYER_JUMP);
	}
	if (actor->pos.y > game_state.water && (game_state.time % 5L) == 0L)
		if (rng(10L) == 5L)
			create_actor(
				ACT_BUBBLE, POS_ADD(actor, FxZero, FfInt((player->power == POW_SMALL) ? 18L : 39L)));

	if (jumped && VAL(actor, PLAYER_GROUND) <= 0L && VAL(actor, Y_SPEED) > FxZero)
		FLAG_ON(actor, FLG_PLAYER_JUMP);

	actor->box.start.y
		= (player->power == POW_SMALL || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? FfInt(-25L) : FfInt(-51L);

	VAL_TICK(actor, PLAYER_GROUND);

	if (ANY_FLAG(actor, FLG_PLAYER_ASCEND)) {
		move_actor(actor, POS_SPEED(actor));
		if (VAL(actor, Y_SPEED) >= FxZero)
			FLAG_OFF(actor, FLG_PLAYER_ASCEND);
	} else {
		displace_actor(actor, FfInt(10L), true);
		if (VAL(actor, Y_TOUCH) > 0L)
			VAL(actor, PLAYER_GROUND) = GROUND_TIME;

		const GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll != NULL) {
			if (game_state.sequence.type != SEQ_WIN) {
				if ((actor->pos.x + actor->box.start.x) < game_state.bounds.start.x) {
					move_actor(actor,
						(fvec2){game_state.bounds.start.x - actor->box.start.x, actor->pos.y});
					VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED), FxZero);
					VAL(actor, X_TOUCH) = -1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
				if ((actor->pos.x + actor->box.end.x) > game_state.bounds.end.x) {
					move_actor(actor,
						(fvec2){(game_state.bounds.end.x - actor->box.end.x), actor->pos.y});
					VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED), FxZero);
					VAL(actor, X_TOUCH) = 1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
			}

			if (ANY_FLAG(autoscroll, FLG_SCROLL_TANKS)
				&& ((actor->pos.y + actor->box.end.y) >= (game_state.bounds.end.y - FfInt(64L))
					|| VAL(actor, PLAYER_GROUND) <= 0L)
				&& !touching_solid(HITBOX_ADD(actor, VAL(autoscroll, X_SPEED), FxZero), SOL_SOLID))
				move_actor(actor, POS_ADD(actor, VAL(autoscroll, X_SPEED), FxZero));
		}
	}

	if (VAL(actor, Y_TOUCH) <= 0L) {
		if (actor->pos.y >= game_state.water) {
			if (VAL(actor, Y_SPEED) > FfInt(3L))
				VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) - FxOne, FfInt(3L));
			else if (VAL(actor, Y_SPEED) < FfInt(3L))
				VAL(actor, Y_SPEED) += 6554L;
		} else if (VAL(actor, Y_SPEED) > FfInt(10L)) {
			VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) - FxOne, FfInt(10L));
		} else if (VAL(actor, Y_SPEED) < FfInt(10L))
			VAL(actor, Y_SPEED) += FxOne;
	}

	// Animation
	if (VAL(actor, X_SPEED) > FxZero && (ANY_INPUT(player, GI_RIGHT) || game_state.sequence.type == SEQ_WIN))
		FLAG_OFF(actor, FLG_X_FLIP);
	else if (VAL(actor, X_SPEED) < FxZero && (ANY_INPUT(player, GI_LEFT) || game_state.sequence.type == SEQ_WIN))
		FLAG_ON(actor, FLG_X_FLIP);

	if (VAL(actor, PLAYER_POWER) > 0L)
		VAL(actor, PLAYER_POWER) -= 91L;
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

		ActorID midx;
		for (; midx < MAX_MISSILES; midx++) {
			GameActor* missile = get_actor(player->missiles[midx]);
			if (missile == NULL)
				goto new_missile;
		}
		break;

	new_missile:
		GameActorType mtype;
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

		GameActor* missile = create_actor(
			mtype, POS_ADD(actor, FfInt(ANY_FLAG(actor, FLG_X_FLIP) ? -5L : 5L), FfInt(-29L)));
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
			VAL(missile, Y_SPEED) = FfInt(-5L);
			break;
		}

		case ACT_MISSILE_HAMMER: {
			FLAG_ON(missile, actor->flags & FLG_X_FLIP);
			VAL(missile, X_SPEED)
				= (ANY_FLAG(actor, FLG_X_FLIP) ? -173015L : 173015L) + VAL(actor, X_SPEED);
			VAL(missile, Y_SPEED) = FfInt(-7L) + Fmin(0L, Fhalf(VAL(actor, Y_SPEED)));
			break;
		}
		}

		if (VAL(actor, PLAYER_GROUND) > 0L)
			VAL(actor, PLAYER_FIRE) = 2L;
		play_actor_sound(actor, "fire");
		break;
	}

	case POW_LUI: {
		if (VAL(actor, PLAYER_GROUND) > 0L)
			break;

		GameActor* effect = create_actor(ACT_PLAYER_EFFECT, actor->pos);
		if (effect == NULL)
			return;

		align_interp(effect, actor);
		FLAG_ON(effect, actor->flags & FLG_X_FLIP);
		VAL(effect, PLAYER_EFFECT_POWER) = player->power;
		VAL(effect, PLAYER_EFFECT_FRAME) = get_player_frame(actor);
		break;
	}
	}

	VAL_TICK(actor, PLAYER_FLASH);

	if (VAL(actor, PLAYER_STARMAN) > 0L) {
		--VAL(actor, PLAYER_STARMAN);
		if (VAL(actor, PLAYER_STARMAN) == 100L)
			play_actor_sound(actor, "starman");
		if (VAL(actor, PLAYER_STARMAN) <= 0L) {
			VAL(actor, PLAYER_STARMAN_COMBO) = 0L;

			// !!! CLIENT-SIDE !!!
			if (VAL(actor, PLAYER_INDEX) == localplayer())
				stop_state_track(TS_POWER);
			// !!! CLIENT-SIDE !!!
		}
	}

	if (actor->pos.y
		> (((get_actor(game_state.autoscroll) != NULL) ? game_state.bounds.end.y : player->bounds.end.y)
			+ FfInt(64L)))
	{
		kill_player(actor);
		goto sync_pos;
	}

	// TODO: Kevin

	collide_actor(actor);
	FLAG_OFF(actor, FLG_PLAYER_STOMP);

sync_pos:
	player->pos.x = actor->pos.x;
	player->pos.y = actor->pos.y;
}

static void draw(const GameActor* actor) {
	GamePlayer* player = get_player((PlayerID)VAL(actor, PLAYER_INDEX));
	if (player == NULL || (VAL(actor, PLAYER_FLASH) % 2L) > 0L)
		return;

	const char* tex = get_player_texture(player->power, get_player_frame(actor));
	// !!! CLIENT-SIDE !!!
	const GLfloat a = (localplayer() == player->id) ? 255L : 191L;
	// !!! CLIENT-SIDE !!!
	draw_actor(actor, tex, 0.f, ALPHA(a));

	if (VAL(actor, PLAYER_STARMAN) > 0L) {
		GLubyte r, g, b;
		switch (game_state.time % 5L) {
		default: {
			r = 248L;
			g = 0L;
			b = 0L;
			break;
		}
		case 1: {
			r = 192L;
			g = 152L;
			b = 0L;
			break;
		}
		case 2: {
			r = 144L;
			g = 192L;
			b = 40L;
			break;
		}
		case 3: {
			r = 88L;
			g = 136L;
			b = 232L;
			break;
		}
		case 4: {
			r = 192L;
			g = 120L;
			b = 200L;
			break;
		}
		}

		batch_stencil(1.f);
		batch_blendmode(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
		draw_actor(actor, tex, 0.f, RGBA(r, g, b, a));
		batch_blendmode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
		batch_stencil(0.f);
	}

	// !!! CLIENT-SIDE !!!
	if (viewplayer() == player->id)
		return;
	const char* name = get_peer_name(player_to_peer(player->id));
	if (name == NULL)
		return;
	const InterpActor* iactor = get_interp(actor);
	batch_cursor(XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y) + FtFloat(actor->box.start.y) - 10.f, -1000.f));
	batch_color(ALPHA(160L));
	batch_align(FA_CENTER, FA_BOTTOM);
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
	if (from->type != ACT_PLAYER || get_actor((ActorID)VAL(from, PLAYER_WARP)) != NULL)
		return;

	if (VAL(from, PLAYER_GROUND) <= 0L && VAL(from, Y_SPEED) > FxZero && from->pos.y < (actor->pos.y - FfInt(10L)))
	{
		GamePlayer* fplayer = get_owner(from);
		VAL(from, Y_SPEED) = FfInt((fplayer != NULL && ANY_INPUT(fplayer, GI_JUMP)) ? -13L : -8L);
		FLAG_ON(from, FLG_PLAYER_STOMP); // In case they overlap an enemy

		VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
		play_actor_sound(actor, "stomp");
	} else if (Fabs(VAL(from, X_SPEED)) > Fabs(VAL(actor, X_SPEED)))
		if ((VAL(from, X_SPEED) > FxZero && from->pos.x < actor->pos.x)
			|| (VAL(from, X_SPEED) < FxZero && from->pos.x > actor->pos.x))
		{
			VAL(actor, X_SPEED) += Fhalf(VAL(from, X_SPEED));
			VAL(from, X_SPEED) = -Fhalf(VAL(from, X_SPEED));
			if (Fabs(VAL(from, X_SPEED)) > FfInt(2L))
				play_actor_sound(actor, "bump");
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
	load_texture("player/mario/dead");

	load_sound("lose");
	load_sound("dead");
	load_sound("hardcore");

	load_track("lose");
	load_track("lose2");
	load_track("game_over");
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
			if (autoscroll != NULL && !ANY_FLAG(autoscroll, FLG_SCROLL_TANKS))
				VAL(autoscroll, X_SPEED) = VAL(autoscroll, Y_SPEED) = FxZero;

			if (game_state.flags & GF_HARDCORE)
				play_state_sound("hardcore");

			for (TrackSlots i = 0; i < (TrackSlots)TS_SIZE; i++)
				stop_state_track(i);
			play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? "lose2" : "lose", false);
		} else
			play_actor_sound(actor, (player->lives >= 0L) ? "lose" : "dead");
		break;
	}

	case 25L:
		VAL(actor, Y_SPEED) = FfInt(-10L);
		break;

	case 150L: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD) || (game_state.flags & GF_SINGLE))
			break;

		GameActor* pawn = respawn_player(player);
		if (pawn != NULL) {
			play_actor_sound(pawn, "respawn");
			VAL(pawn, PLAYER_FLASH) = 100L;
			FLAG_ON(actor, FLG_DESTROY);
		}

		break;
	}

	case 200L: {
		if (game_state.sequence.type == SEQ_LOSE)
			for (PlayerID i = 0; i < numplayers(); i++) {
				GamePlayer* p = get_player(i);
				if (p != NULL && p->lives >= 0L) {
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
			play_state_track(TS_FANFARE, "game_over", false);
		}

		FLAG_ON(actor, FLG_DESTROY);
		break;
	}
	}
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "player/mario/dead", 0.f, ALPHA((localplayer() == get_owner_id(actor)) ? 255L : 191L));

	if (!ANY_FLAG(actor, FLG_PLAYER_JACKASS))
		return;
	const InterpActor* iactor = get_interp(actor);
	batch_start(XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y + actor->box.start.y) - 32.f, -1000.f), 0.f, WHITE);
	batch_align(FA_CENTER, FA_BOTTOM);
	batch_string("main", 24, "Jackass");
}

const GameActorTable TAB_PLAYER_DEAD = {
	.load = load_corpse,
	.create = create_corpse,
	.tick = tick_corpse,
	.draw = draw_corpse,
	.owner = owner,
};
