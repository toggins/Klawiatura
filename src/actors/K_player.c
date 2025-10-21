#include "K_player.h"

// ===========
// SPAWN POINT
// ===========

static void create_spawn(GameActor* actor) {
	game_state.spawn = actor->id;
}

static void cleanup_spawn(GameActor* actor) {
	if (game_state.spawn == actor->id)
		game_state.spawn = NULLACT;
}

const GameActorTable TAB_PLAYER_SPAWN = {.create = create_spawn, .cleanup = cleanup_spawn};

// ====
// PAWN
// ====

static void load() {
	load_texture("markers/player");

	load_sound("jump");
	load_sound("swim");
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
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
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
		const fixed vx = actor->values[VAL_X_SPEED];
		const fixed vy = actor->values[VAL_Y_SPEED];

		actor->values[VAL_X_SPEED] = platform->values[VAL_X_SPEED];
		actor->values[VAL_Y_SPEED] = platform->values[VAL_Y_SPEED];
		displace_actor(actor, FxZero, false);
		actor->values[VAL_X_SPEED] = vx;
		actor->values[VAL_Y_SPEED] = vy;

		const frect mbox = HITBOX_ADD(actor, platform->values[VAL_X_SPEED], platform->values[VAL_Y_SPEED]);
		const frect pbox = HITBOX(platform);
		actor->values[VAL_PLAYER_PLATFORM] = Rcollide(mbox, pbox) ? pfid : NULLACT;
	}

	const Bool cant_run = !ANY_INPUT(player, GI_RUN) || actor->pos.y >= game_state.water;
	const Bool jumped = ANY_PRESSED(player, GI_JUMP);

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

					if (touching_solid(HITBOX(actor), SOL_SOLID)) {
						// kill_player(actor);
					}
				}
				if ((actor->pos.x + actor->box.end.x) > game_state.bounds.end.x) {
					move_actor(actor,
						(fvec2){(game_state.bounds.end.x - actor->box.end.x), actor->pos.y});
					actor->values[VAL_X_SPEED] = Fmin(actor->values[VAL_X_SPEED], FxZero);
					actor->values[VAL_X_TOUCH] = 1L;

					if (touching_solid(HITBOX(actor), SOL_SOLID)) {
						// kill_player(actor);
					}
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
	// TODO: Starman

	// TODO: Pit

	// TODO: Kevin

	collide_actor(actor);
	FLAG_OFF(actor, FLG_PLAYER_STOMP);

	player->pos.x = actor->pos.x;
	player->pos.y = actor->pos.y;
}

static void draw(const GameActor* actor) {
	draw_actor(actor, "markers/player", 0, WHITE);
}

static void cleanup(GameActor* actor) {
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
	if (player == NULL)
		return;
	if (player->actor == actor->id)
		player->actor = NULLACT;
}

static PlayerID owner(GameActor* actor) {
	return (PlayerID)actor->values[VAL_PLAYER_INDEX];
}

const GameActorTable TAB_PLAYER
	= {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup, .owner = owner};
