#include "K_player.h"
#include "K_points.h"

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
	//     const GameActor* warp = get_actor((ActorID)actor->values[VAL_PLAYER_WARP]);
	//     if (warp != NULL)
	//         switch (warp->values[VAL_WARP_ANGLE]) {
	//             case 0:
	//             case 2:
	//                 switch (FtInt(actor->values[VAL_PLAYER_FRAME]) % 3L) {
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

	if (actor->values[VAL_PLAYER_POWER] > 0L) {
		const GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
		switch ((player == NULL) ? POW_SMALL : player->power) {
		case POW_SMALL:
		case POW_BIG: {
			switch ((actor->values[VAL_PLAYER_POWER] / 100L) % 3L) {
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
			switch ((actor->values[VAL_PLAYER_POWER] / 100L) % 4L) {
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

	if (actor->values[VAL_PLAYER_GROUND] <= 0L) {
		if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			const int32_t frame = FtInt(actor->values[VAL_PLAYER_FRAME]);
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
			return (actor->values[VAL_Y_SPEED] < FxZero) ? PF_JUMP : PF_FALL;
	}

	if (ANY_FLAG(actor, FLG_PLAYER_DUCK))
		return PF_DUCK;

	if (actor->values[VAL_PLAYER_FIRE] > 0L)
		return PF_FIRE;

	if (actor->values[VAL_X_SPEED] != FxZero)
		switch (FtInt(actor->values[VAL_PLAYER_FRAME]) % 3L) {
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
	if (actor->type != ACT_PLAYER || actor->values[VAL_PLAYER_FLASH] > 0L || actor->values[VAL_PLAYER_STARMAN] > 0L
		|| actor->values[VAL_PLAYER_WARP] > 0L || game_state.sequence.type == SEQ_WIN)
		return false;

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		if (player->power == POW_SMALL) {
			kill_player(actor);
			return true;
		}
		player->power = (player->power == POW_BIG) ? POW_SMALL : POW_BIG;
	}

	actor->values[VAL_PLAYER_POWER] = 3000L, actor->values[VAL_PLAYER_FLASH] = 100L;
	play_actor_sound(actor, "warp");
	return true;
}

void kill_player(GameActor* actor) {
	if (actor->type != ACT_PLAYER)
		return;

	GameActor* dead = create_actor(ACT_PLAYER_DEAD, actor->pos);
	if (dead == NULL)
		return;

	// !!! CLIENT-SIDE !!!
	if (actor->values[VAL_PLAYER_INDEX] == localplayer() && actor->values[VAL_PLAYER_STARMAN] > 0L)
		stop_state_track(TS_POWER);
	// !!! CLIENT-SIDE !!!

	GamePlayer* player = get_owner(actor);
	if (player != NULL) {
		dead->values[VAL_PLAYER_INDEX] = (ActorValue)player->id;

		--player->lives;
		player->power = POW_SMALL;

		GameActor* kpawn = get_actor(player->kevin.actor);
		if (kpawn != NULL) {
			create_actor(ACT_EXPLODE, POS_ADD(kpawn, FxZero, FfInt(-16L)));
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
	set_view_player(player);

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
	actor->values[VAL_PLAYER_FLASH] = actor->values[VAL_PLAYER_STARMAN] = 0L;
	for (TrackSlots i = 0L; i < (TrackSlots)TS_SIZE; i++)
		stop_state_track(i);
	play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? "win2" : "win", false);
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
	load_sound("swim");
	load_sound("warp");
	load_sound("starman");
	load_sound("respawn");

	load_track("win");
	load_track("win2");
	load_track("win3");

	load_actor(ACT_PLAYER_DEAD);
}

static void create(GameActor* actor) {
	actor->depth = -FxOne;

	actor->box.start.x = FfInt(-9L);
	actor->box.start.y = FfInt(-25L);
	actor->box.end.x = FfInt(10L);
	actor->box.end.y = FxOne;

	actor->values[VAL_PLAYER_INDEX] = (ActorValue)NULLPLAY;
	actor->values[VAL_PLAYER_GROUND] = 2L;
	actor->values[VAL_PLAYER_WARP] = NULLACT;
	actor->values[VAL_PLAYER_PLATFORM] = NULLACT;
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
		actor->values[VAL_X_SPEED] = 163840L;
	}

	const ActorID pfid = (ActorID)(actor->values[VAL_PLAYER_PLATFORM]);
	GameActor* platform = get_actor(pfid);
	if (platform != NULL) {
		const fixed vx = actor->values[VAL_X_SPEED], vy = actor->values[VAL_Y_SPEED];

		actor->values[VAL_X_SPEED] = platform->values[VAL_X_SPEED],
		actor->values[VAL_Y_SPEED] = platform->values[VAL_Y_SPEED];
		displace_actor(actor, FxZero, false);
		actor->values[VAL_X_SPEED] = vx, actor->values[VAL_Y_SPEED] = vy;

		const frect mbox = HITBOX_ADD(actor, platform->values[VAL_X_SPEED], platform->values[VAL_Y_SPEED]),
			    pbox = HITBOX(platform);
		actor->values[VAL_PLAYER_PLATFORM] = Rcollide(mbox, pbox) ? pfid : NULLACT;
	}

	const Bool cant_run = (!ANY_INPUT(player, GI_RUN) || actor->pos.y >= game_state.water),
		   jumped = ANY_PRESSED(player, GI_JUMP);

	if (ANY_INPUT(player, GI_RIGHT) && actor->values[VAL_X_TOUCH] <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] >= FxZero
		&& ((cant_run && actor->values[VAL_X_SPEED] < 286720L)
			|| (!cant_run && actor->values[VAL_X_SPEED] < 491520L)))
		actor->values[VAL_X_SPEED] += 8192L;
	if (ANY_INPUT(player, GI_LEFT) && actor->values[VAL_X_TOUCH] >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] <= FxZero
		&& ((cant_run && actor->values[VAL_X_SPEED] > -286720)
			|| (!cant_run && actor->values[VAL_X_SPEED] > -491520L)))
		actor->values[VAL_X_SPEED] -= 8192L;

	if (!ANY_INPUT(player, GI_RIGHT) && actor->values[VAL_X_SPEED] > FxZero)
		actor->values[VAL_X_SPEED] = Fmax(actor->values[VAL_X_SPEED] - 8192L, FxZero);
	if (!ANY_INPUT(player, GI_LEFT) && actor->values[VAL_X_SPEED] < FxZero)
		actor->values[VAL_X_SPEED] = Fmin(actor->values[VAL_X_SPEED] + 8192L, FxZero);

	if (ANY_INPUT(player, GI_RIGHT) && actor->values[VAL_X_TOUCH] <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] < FxZero)
		actor->values[VAL_X_SPEED] += 24576L;
	if (ANY_INPUT(player, GI_LEFT) && actor->values[VAL_X_TOUCH] >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] > FxZero)
		actor->values[VAL_X_SPEED] -= 24576L;

	if (ANY_INPUT(player, GI_RIGHT) && actor->values[VAL_X_TOUCH] <= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] < FxOne && actor->values[VAL_X_SPEED] >= FxZero)
		actor->values[VAL_X_SPEED] += FxOne;
	if (ANY_INPUT(player, GI_LEFT) && actor->values[VAL_X_TOUCH] >= 0L && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
		&& actor->values[VAL_X_SPEED] > -FxOne && actor->values[VAL_X_SPEED] <= FxZero)
		actor->values[VAL_X_SPEED] -= FxOne;

	if (cant_run && Fabs(actor->values[VAL_X_SPEED]) > 286720L)
		actor->values[VAL_X_SPEED] -= (actor->values[VAL_X_SPEED] >= FxZero) ? 8192L : -8192L;

	if (ANY_INPUT(player, GI_DOWN) && !ANY_INPUT(player, GI_LEFT | GI_RIGHT)
		&& actor->values[VAL_PLAYER_GROUND] > 0L && player->power != POW_SMALL)
		FLAG_ON(actor, FLG_PLAYER_DUCK);
	if (ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
		if (!ANY_INPUT(player, GI_DOWN) || player->power == POW_SMALL)
			FLAG_OFF(actor, FLG_PLAYER_DUCK);
		else if (actor->values[VAL_X_SPEED] > FxZero)
			actor->values[VAL_X_SPEED] = Fmax(actor->values[VAL_X_SPEED] - 24576L, FxZero);
		else if (actor->values[VAL_X_SPEED] < FxZero)
			actor->values[VAL_X_SPEED] = Fmin(actor->values[VAL_X_SPEED] + 24576L, FxZero);
	}

	if (ANY_INPUT(player, GI_JUMP) && actor->values[VAL_Y_SPEED] < FxZero && actor->pos.y < game_state.water
		&& !ANY_INPUT(player, GI_DOWN))
		actor->values[VAL_Y_SPEED] -= (player->power == POW_LUI)
		                                      ? 39322L
		                                      : ((Fabs(actor->values[VAL_X_SPEED]) < 40960L) ? 26214L : FxHalf);

	if (jumped) {
		actor->values[VAL_PLAYER_SPRING] = 7L;
		if (actor->pos.y >= game_state.water && !ANY_INPUT(player, GI_DOWN)) {
			actor->values[VAL_Y_TOUCH] = 0L;
			actor->values[VAL_Y_SPEED]
				= FfInt(((actor->pos.y + actor->box.start.y)
						> (game_state.water + FfInt(16L)
							|| (actor->pos.y + actor->box.end.y) < game_state.water))
						? -3L
						: -9L);
			actor->values[VAL_PLAYER_GROUND] = 0L;
			actor->values[VAL_PLAYER_FRAME] = FxZero;
			play_actor_sound(actor, "swim");
		}
	}
	if (actor->values[VAL_PLAYER_SPRING] > 0L)
		--actor->values[VAL_PLAYER_SPRING];

	if (actor->pos.y >= game_state.water) {
		if (!ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
			if (Fabs(actor->values[VAL_X_SPEED]) >= FxHalf)
				actor->values[VAL_X_SPEED] -= (actor->values[VAL_X_SPEED] >= FxZero) ? FxHalf : -FxHalf;
			actor->values[VAL_PLAYER_FRAME] = FxZero;
			if (actor->pos.y < (game_state.water + FfInt(11L)))
				create_actor(ACT_WATER_SPLASH, actor->pos);
			FLAG_ON(actor, FLG_PLAYER_SWIM);
		}
	} else if (ANY_FLAG(actor, FLG_PLAYER_SWIM)) {
		actor->values[VAL_PLAYER_FRAME] = FxZero;
		FLAG_OFF(actor, FLG_PLAYER_SWIM);
	}

	if (actor->pos.y < game_state.water) {
		if ((jumped || (ANY_INPUT(player, GI_JUMP) && ANY_FLAG(actor, FLG_PLAYER_JUMP)))
			&& !ANY_INPUT(player, GI_DOWN) && actor->values[VAL_PLAYER_GROUND] > 0L)
		{
			actor->values[VAL_PLAYER_GROUND] = 0L;
			actor->values[VAL_Y_SPEED] = FfInt(-13L);
			FLAG_OFF(actor, FLG_PLAYER_JUMP);
			play_actor_sound(actor, "jump");
		}
		if (!ANY_INPUT(player, GI_JUMP) && !ANY_INPUT(player, GI_DOWN) && actor->values[VAL_PLAYER_GROUND] > 0L
			&& ANY_FLAG(actor, FLG_PLAYER_JUMP))
			FLAG_OFF(actor, FLG_PLAYER_JUMP);
	}
	if (actor->pos.y > game_state.water && (game_state.time % 5L) == 0L)
		if (rng(10L) == 5L)
			create_actor(
				ACT_BUBBLE, POS_ADD(actor, FxZero, FfInt((player->power == POW_SMALL) ? 18L : 39L)));

	if (jumped && actor->values[VAL_PLAYER_GROUND] <= 0L && actor->values[VAL_Y_SPEED] > FxZero)
		FLAG_ON(actor, FLG_PLAYER_JUMP);

	actor->box.start.y
		= (player->power == POW_SMALL || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? FfInt(-25L) : FfInt(-51L);

	if (actor->values[VAL_PLAYER_GROUND] > 0L)
		--actor->values[VAL_PLAYER_GROUND];

	if (ANY_FLAG(actor, FLG_PLAYER_ASCEND)) {
		move_actor(actor, POS_SPEED(actor));
		if (actor->values[VAL_Y_SPEED] >= FxZero)
			FLAG_OFF(actor, FLG_PLAYER_ASCEND);
	} else {
		displace_actor(actor, FfInt(10L), true);
		if (actor->values[VAL_Y_TOUCH] > 0L)
			actor->values[VAL_PLAYER_GROUND] = 2L;

		const GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll != NULL) {
			if (game_state.sequence.type != SEQ_WIN) {
				if ((actor->pos.y + actor->box.start.x) < game_state.bounds.start.x) {
					move_actor(actor,
						(fvec2){game_state.bounds.start.x - actor->box.start.x, actor->pos.y});
					actor->values[VAL_X_SPEED] = Fmax(actor->values[VAL_X_SPEED], FxZero);
					actor->values[VAL_X_TOUCH] = -1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
				if ((actor->pos.x + actor->box.end.x) > game_state.bounds.end.x) {
					move_actor(actor,
						(fvec2){(game_state.bounds.end.x - actor->box.end.x), actor->pos.y});
					actor->values[VAL_X_SPEED] = Fmin(actor->values[VAL_X_SPEED], FxZero);
					actor->values[VAL_X_TOUCH] = 1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID))
						kill_player(actor);
				}
			}

			// if ((autoscroll->flags & FLG_SCROLL_TANKS)
			// 	&& (Fadd(object->pos[1], object->bbox[1][1]) >= Fsub(state.bounds[1][1], FfInt(64L))
			// 		|| object->values[VAL_PLAYER_GROUND] <= 0L)
			// 	&& !touching_solid(HITBOX_ADD(object, autoscroll->values[VAL_X_SPEED], FxZero)))
			// 	move_object(oid, POS_ADD(object, autoscroll->values[VAL_X_SPEED], FxZero));
		}
	}

	const Bool carried = ANY_FLAG(actor, FLG_PLAYER_CARRIED);
	if (actor->values[VAL_Y_TOUCH] <= 0L || carried) {
		if (actor->pos.y >= game_state.water) {
			if (actor->values[VAL_Y_SPEED] > FfInt(3L) && !carried) {
				actor->values[VAL_Y_SPEED] = Fmin(actor->values[VAL_Y_SPEED] - FxOne, FfInt(3L));
			} else if (actor->values[VAL_Y_SPEED] < FfInt(3L) || carried) {
				actor->values[VAL_Y_SPEED] += 6554L;
				FLAG_OFF(actor, FLG_PLAYER_CARRIED);
			}
		} else if (actor->values[VAL_Y_SPEED] > FfInt(10L) && !carried) {
			actor->values[VAL_Y_SPEED] = Fmin(actor->values[VAL_Y_SPEED] - FxOne, FfInt(10L));
		} else if (actor->values[VAL_Y_SPEED] < FfInt(10L) || carried) {
			actor->values[VAL_Y_SPEED] += FxOne;
			FLAG_OFF(actor, FLG_PLAYER_CARRIED);
		}
	}

	// Animation
	if (actor->values[VAL_X_SPEED] > FxZero && (ANY_INPUT(player, GI_RIGHT) || game_state.sequence.type == SEQ_WIN))
		FLAG_OFF(actor, FLG_X_FLIP);
	else if (actor->values[VAL_X_SPEED] < FxZero
		 && (ANY_INPUT(player, GI_LEFT) || game_state.sequence.type == SEQ_WIN))
		FLAG_ON(actor, FLG_X_FLIP);

	if (actor->values[VAL_PLAYER_POWER] > 0L)
		actor->values[VAL_PLAYER_POWER] -= 91L;
	if (actor->values[VAL_PLAYER_FIRE] > 0L)
		--actor->values[VAL_PLAYER_FIRE];

	if (actor->values[VAL_PLAYER_GROUND] > 0L)
		if (actor->values[VAL_X_SPEED] == FxZero)
			actor->values[VAL_PLAYER_FRAME] = FxZero;
		else
			actor->values[VAL_PLAYER_FRAME]
				+= Fclamp(Fmul(Fabs(actor->values[VAL_X_SPEED]), 5243L), 9175L, FxHalf);
	else if (ANY_FLAG(actor, FLG_PLAYER_SWIM))
		actor->values[VAL_PLAYER_FRAME] += Fclamp(Fmul(Fabs(actor->values[VAL_X_SPEED]), 5243L), 9175L, 16056L);
	else
		actor->values[VAL_PLAYER_FRAME] = FxZero;

	if (actor->values[VAL_PLAYER_FLASH] > 0L)
		--actor->values[VAL_PLAYER_FLASH];

	if (actor->values[VAL_PLAYER_STARMAN] > 0L) {
		--actor->values[VAL_PLAYER_STARMAN];
		const int32_t starman = actor->values[VAL_PLAYER_STARMAN];
		if (starman == 100L)
			play_actor_sound(actor, "starman");
		if (starman <= 0L) {
			actor->values[VAL_PLAYER_STARMAN_COMBO] = 0L;

			// !!! CLIENT-SIDE !!!
			if (actor->values[VAL_PLAYER_INDEX] == localplayer())
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
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
	if (player == NULL || (actor->values[VAL_PLAYER_FLASH] % 2L) > 0L)
		return;

	const char* tex = get_player_texture(player->power, get_player_frame(actor));
	const GLfloat a = (localplayer() == player->id) ? 255L : 191L;
	draw_actor(actor, tex, 0.f, ALPHA(a));

	if (actor->values[VAL_PLAYER_STARMAN] > 0L) {
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

	if (viewplayer() != player->id) {
		const char* name = get_peer_name(player->id);
		if (name == NULL)
			return;
		const InterpActor* iactor = get_interp(actor);
		batch_cursor(
			XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y) + FtFloat(actor->box.start.y) - 10.f, -1000.f));
		batch_color(ALPHA(160L));
		batch_align(FA_CENTER, FA_BOTTOM);
		batch_string("main", 20, name);
	}
}

static void cleanup(GameActor* actor) {
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
	if (player == NULL)
		return;
	if (player->actor == actor->id)
		player->actor = NULLACT;
}

static PlayerID owner(const GameActor* actor) {
	return (PlayerID)actor->values[VAL_PLAYER_INDEX];
}

const GameActorTable TAB_PLAYER
	= {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup, .owner = owner};

// ===========
// DEAD PLAYER
// ===========

static void load_dead() {
	load_texture("player/mario/dead");

	load_sound("lose");
	load_sound("dead");
	load_sound("hardcore");

	load_track("lose");
	load_track("lose2");
	load_track("game_over");
}

static void create_dead(GameActor* actor) {
	actor->depth = -FxOne;
	actor->values[VAL_PLAYER_INDEX] = (ActorValue)NULLPLAY;
}

static void tick_dead(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (actor->values[VAL_PLAYER_FRAME] >= 25L) {
		actor->values[VAL_Y_SPEED] += 26214L;
		move_actor(actor, POS_SPEED(actor));
	}

	switch ((actor->values[VAL_PLAYER_FRAME])++) {
	default:
		break;

	case 0: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD)) {
			for (TrackSlots i = 0; i < (TrackSlots)TS_SIZE; i++)
				stop_state_track(i);
			play_state_track(TS_FANFARE, (game_state.flags & GF_LOST) ? "lose2" : "lose", false);
			if (game_state.flags & GF_HARDCORE)
				play_state_sound("hardcore");
		} else
			play_actor_sound(actor, (player->lives >= 0L) ? "lose" : "dead");
		break;
	}

	case 25:
		actor->values[VAL_Y_SPEED] = FfInt(-10L);
		break;

	case 150: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD) || (game_state.flags & GF_SINGLE))
			break;

		GameActor* pawn = respawn_player(player);
		if (pawn != NULL) {
			play_actor_sound(pawn, "respawn");
			pawn->values[VAL_PLAYER_FLASH] = 100L;
			FLAG_ON(actor, FLG_DESTROY);
		}

		break;
	}

	case 200: {
		for (PlayerID i = 0; i < numplayers(); i++) {
			GamePlayer* p = get_player(i);
			if (p != NULL && p->lives >= 0L) {
				game_state.flags |= GF_RESTART;
				break;
			}
		}

		break;
	}

	case 210: {
		if (ANY_FLAG(actor, FLG_PLAYER_DEAD) || game_state.clock == 0L) {
			game_state.sequence.type = SEQ_LOSE;
			game_state.sequence.time = 1L;
			play_state_track(TS_FANFARE, "game_over", false);
		}

		FLAG_ON(actor, FLG_DESTROY);
		break;
	}
	}
}

static void draw_dead(const GameActor* actor) {
	draw_actor(actor, "player/mario/dead", 0.f, ALPHA((localplayer() == get_owner_id(actor)) ? 255L : 191L));

	if (!ANY_FLAG(actor, FLG_PLAYER_JACKASS))
		return;
	const InterpActor* iactor = get_interp(actor);
	batch_start(XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y + actor->box.start.y) - 32.f, -1000.f), 0.f, WHITE);
	batch_align(FA_CENTER, FA_BOTTOM);
	batch_string("main", 24, "Jackass");
}

const GameActorTable TAB_PLAYER_DEAD
	= {.load = load_dead, .create = create_dead, .tick = tick_dead, .draw = draw_dead, .owner = owner};
