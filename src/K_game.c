#include "K_cmd.h"
#include "K_file.h"
#include "K_game.h"
#include "K_input.h"
#include "K_log.h"
#include "K_menu.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "actors/K_block.h"
#include "actors/K_checkpoint.h"
#include "actors/K_player.h"

#define ACTOR_TYPE_CALL(type, fn)                                                                                      \
	do {                                                                                                           \
		if (ACTORS[(type)] != NULL && ACTORS[(type)]->fn != NULL)                                              \
			ACTORS[(type)]->fn();                                                                          \
	} while (0)

#define ACTOR_CALL(act, fn)                                                                                            \
	do {                                                                                                           \
		if (ACTORS[(act)->type] != NULL && ACTORS[(act)->type]->fn != NULL)                                    \
			ACTORS[(act)->type]->fn((act));                                                                \
	} while (0)

#define ACTOR_CALL2(act, fn, act2)                                                                                     \
	do {                                                                                                           \
		if (ACTORS[(act)->type] != NULL && ACTORS[(act)->type]->fn != NULL)                                    \
			ACTORS[(act)->type]->fn((act), (act2));                                                        \
	} while (0)

#define ACTOR_GET_SOLID(act)                                                                                           \
	((ACTORS[(act)->type] != NULL && ACTORS[(act)->type]->is_solid != NULL) ? ACTORS[(act)->type]->is_solid(act)   \
										: 0)

#define ACTOR_IS_SOLID(act, types) (ACTOR_GET_SOLID(act) & (types))

SolidType always_solid(const GameActor* actor) {
	return SOL_SOLID;
}

SolidType always_top(const GameActor* actor) {
	return SOL_TOP;
}

SolidType always_bottom(const GameActor* actor) {
	return SOL_BOTTOM;
}

static const GameActorTable* ACTORS[ACT_SIZE] = {0};
#define ACTOR(ident)                                                                                                   \
	extern const GameActorTable TAB_##ident;                                                                       \
	ACTORS[ACT_##ident] = &TAB_##ident;

void populate_actors_table() {
	ACTOR(PLAYER_SPAWN);
	ACTOR(PLAYER);
	ACTOR(PLAYER_DEAD);
	ACTOR(MISSILE_FIREBALL);
	ACTOR(CLOUD);
	ACTOR(BUSH);
	ACTOR(SOLID);
	ACTOR(SOLID_TOP);
	ACTOR(SOLID_SLOPE);
	ACTOR(WHEEL_LEFT);
	ACTOR(WHEEL);
	ACTOR(WHEEL_RIGHT);
	ACTOR(COIN);
	ACTOR(COIN_POP);
	ACTOR(POINTS);
	ACTOR(GOAL_BAR);
	ACTOR(GOAL_BAR_FLY);
	ACTOR(GOAL_MARK);
	ACTOR(ITEM_BLOCK);
	ACTOR(HIDDEN_BLOCK);
	ACTOR(BRICK_BLOCK);
	ACTOR(BRICK_SHARD);
	ACTOR(COIN_BLOCK);
	ACTOR(NOTE_BLOCK);
	ACTOR(BLOCK_BUMP);
	ACTOR(PSWITCH);
	ACTOR(PSWITCH_COIN);
	ACTOR(PSWITCH_BRICK);
	ACTOR(MUSHROOM);
	ACTOR(MUSHROOM_1UP);
	ACTOR(MUSHROOM_POISON);
	ACTOR(FIRE_FLOWER);
	ACTOR(STARMAN);
	ACTOR(AUTOSCROLL);
	ACTOR(BOUNDS);
	ACTOR(CHECKPOINT);
	ACTOR(PLATFORM);
	ACTOR(PLATFORM_TURN);
	ACTOR(EXPLODE);

	static const GameActorTable TAB_NULL = {0};
	ACTORS[ACT_NULL] = &TAB_NULL;
}

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;
GameState game_state = {0};

PlayerID local_player = NULLPLAY, view_player = NULLPLAY, num_players = 0L;

static InterpState interp = {0};

// ====
// CHAT
// ===

static const GLfloat chat_fs = 24.f;
static char chat_message[128] = {0};
static struct {
	char text[sizeof(chat_message)];
	int lifetime;
} chat_hist[32] = {0};

static void send_chat_message() {
	if (typing_what() || !SDL_strlen(chat_message))
		return;
	// TODO: use message headers to distinguish message types, instead of using gross hacks such as this.
	static char buf[6 + sizeof(chat_message)] = "CHATTO";
	SDL_strlcpy(buf + 6, chat_message, sizeof(chat_message));
	for (int i = 0; i < MAX_PLAYERS; i++)
		NutPunch_SendReliably(i, buf, sizeof(buf));
	push_chat_message(NutPunch_LocalPeer(), chat_message);
	chat_message[0] = 0;
}

static bool is_hist_null(int idx) {
	if (idx < 0 || idx >= sizeof(chat_hist) / sizeof(*chat_hist))
		return false;
	return !SDL_strlen(chat_hist[idx].text);
}

void push_chat_message(const int from, const char* text) {
	int len = sizeof(chat_hist) / sizeof(*chat_hist);
	for (int i = 0; i < len; i++)
		if (is_hist_null(i)) {
			const char* line = fmt("%s: %s", get_peer_name(from), text);
			SDL_strlcpy(chat_hist[i].text, line, sizeof(chat_message));
			chat_hist[i].lifetime = 5 * TICKRATE;
			return;
		}
}

static void tick_chat_hist() {
	for (int i = 0; i < sizeof(chat_hist) / sizeof(*chat_hist); i++) {
		if (chat_hist[i].lifetime > 0)
			chat_hist[i].lifetime--;
		if (!chat_hist[i].lifetime)
			chat_hist[i].text[0] = 0;
	}
}

static void draw_chat_hist() {
	int height, start = 0, len = sizeof(chat_hist) / sizeof(*chat_hist);
	get_resolution(NULL, &height);

	for (; start < len; start++)
		if (!is_hist_null(start))
			break;

	for (int i = 0; i < len; i++) {
		batch_align(FA_LEFT, FA_BOTTOM);
		batch_cursor(XY(12, height - chat_fs * (1 + i - start)));
		batch_string("main", chat_fs, chat_hist[i].text);
	}
}

static void draw_chat_message() {
	if (typing_what() != chat_message)
		return;

	int height;
	get_resolution(NULL, &height);

	batch_align(FA_LEFT, FA_BOTTOM);
	batch_cursor(XY(12, height));
	batch_string("main", chat_fs, fmt("> %s", chat_message));
}

// ====
// GAME
// ====

/// Initializes a GameContext struct with a clean 1-player preset.
void setup_game_context(GameContext* ctx, const char* level, GameFlag flags) {
	SDL_memset(ctx, 0, sizeof(GameContext));
	ctx->flags = flags;

	ctx->num_players = 1;
	for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
		ctx->players[i].power = POW_SMALL;
		ctx->players[i].lives = DEFAULT_LIVES;
	}

	SDL_strlcpy(ctx->level, level, sizeof(ctx->level));
	ctx->checkpoint = NULLACT;
}

void start_game(GameContext* ctx) {
	if (game_session != NULL)
		return;

	load_texture("ui/sidebar_l");
	load_texture("ui/sidebar_r");

	game_surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, true, true);

	gekko_create(&game_session);

	GekkoConfig cfg = {0};
	cfg.desync_detection = true;
	cfg.input_size = sizeof(GameInput);
	cfg.state_size = sizeof(SaveState);
	cfg.max_spectators = 0;
	cfg.input_prediction_window = MAX_INPUT_DELAY;
	cfg.num_players = num_players = ctx->num_players;

	gekko_start(game_session, &cfg);
	local_player = view_player = populate_game(game_session);
	populate_results();

	start_audio_state();
	start_video_state();
	start_game_state(ctx);
	from_scratch();
	input_wipeout();
}

bool game_exists() {
	return game_session != NULL;
}

void nuke_game() {
	if (game_session == NULL)
		return;

	nuke_game_state();
	nuke_audio_state();
	nuke_video_state();

	gekko_destroy(&game_session);
	game_session = NULL;
	destroy_surface(game_surface);
	game_surface = NULL;
}

bool update_game() {
	gekko_network_poll(game_session);

	tick_chat_hist();
	if (!typing_what() && is_connected()) {
		if (kb_pressed(KB_CHAT))
			start_typing(chat_message, sizeof(chat_message));
		else
			send_chat_message();
	}

	float ftick = 0.f;
	const float ahh = gekko_frames_ahead(game_session), ahead = SDL_clamp(ahh, 0, 2);
	new_frame(ahead);

	if (!got_ticks())
		goto no_tick;

	for (GameActor* actor = get_actor(game_state.live_actors); actor != NULL; actor = get_actor(actor->previous)) {
		InterpActor* iactor = &interp.actors[actor->id];
		if (iactor->type != actor->type) {
			iactor->type = actor->type;
			iactor->from.x = iactor->to.x = iactor->pos.x = actor->pos.x;
			iactor->from.y = iactor->to.y = iactor->pos.y = actor->pos.y;
			continue;
		}

		iactor->from.x = iactor->to.x, iactor->from.y = iactor->to.y;
		iactor->to.x = actor->pos.x, iactor->to.y = actor->pos.y;
	}

	while (got_ticks()) {
		if (kb_pressed(KB_PAUSE))
			goto byebye_game;

		GameInput input = 0;
		if (!typing_what()) {
			input |= kb_down(KB_UP) * GI_UP;
			input |= kb_down(KB_LEFT) * GI_LEFT;
			input |= kb_down(KB_DOWN) * GI_DOWN;
			input |= kb_down(KB_RIGHT) * GI_RIGHT;
			input |= kb_down(KB_JUMP) * GI_JUMP;
			input |= kb_down(KB_FIRE) * GI_FIRE;
			input |= kb_down(KB_RUN) * GI_RUN;
		}
		if (game_state.flags & (GF_END | GF_RESTART))
			input |= GI_WARP;
		gekko_add_local_input(game_session, local_player, &input);

		int count = 0;
		GekkoSessionEvent** events = gekko_session_events(game_session, &count);
		for (int i = 0; i < count; i++) {
			GekkoSessionEvent* event = events[i];
			switch (event->type) {
			case DesyncDetected: {
				dump_game_state();
				struct Desynced desync = event->data.desynced;
				show_error("Out of sync with player %i\n"
					   "Tick: %i\n"
					   "Local Checksum: %u\n"
					   "Remote Checksum: %u",
					desync.remote_handle + 1, desync.frame, desync.local_checksum,
					desync.remote_checksum);
				goto byebye_game;
			}

			case PlayerConnected: {
				struct Connected connect = event->data.connected;
				INFO("Player %i connected", connect.handle + 1);
				break;
			}

			case PlayerDisconnected: {
				struct Disconnected disconnect = event->data.disconnected;
				show_error("Player %i disconnected", disconnect.handle + 1);
				goto byebye_game;
			}

			default:
				break;
			}
		}

		count = 0;
		GekkoGameEvent** updates = gekko_update_session(game_session, &count);
		for (int i = 0; i < count; i++) {
			GekkoGameEvent* event = updates[i];
			switch (event->type) {
			case SaveEvent: {
				static SaveState save;
				save_game_state(&save.game);
				save_audio_state(&save.audio);

				*event->data.save.state_len = sizeof(save);
				*event->data.save.checksum = check_game_state();
				SDL_memcpy(event->data.save.state, &save, sizeof(save));
				break;
			}

			case LoadEvent: {
				const SaveState* load = (SaveState*)(event->data.load.state);
				load_game_state(&load->game);
				load_audio_state(&load->audio);
				break;
			}

			case AdvanceEvent: {
				GameInput inputs[MAX_PLAYERS] = {0};
				for (PlayerID j = 0; j < num_players; j++)
					inputs[j] = ((GameInput*)(event->data.adv.inputs))[j];
				tick_game_state(inputs);
				tick_audio_state();

				for (PlayerID j = 0; j < num_players; j++)
					if (!(inputs[j] & GI_WARP))
						goto no_warp;

				if (game_state.flags & GF_END) {
					if (game_state.next[0] == '\0') {
						show_results();
						goto byebye_game;
					}

					GameContext ctx = {0};
					setup_game_context(&ctx, game_state.next, GF_TRY_SINGLE | GF_TRY_KEVIN);
					ctx.num_players = num_players;
					for (PlayerID i = 0; i < num_players; i++) {
						const int8_t lives = game_state.players[i].lives;
						if (lives < 0L)
							continue;
						ctx.players[i].lives = lives;
						ctx.players[i].power = game_state.players[i].power;
						ctx.players[i].coins = game_state.players[i].coins;
						ctx.players[i].score = game_state.players[i].score;
					}

					nuke_game();
					start_game(&ctx);
					goto break_events;
				} else if (game_state.flags & GF_RESTART) {
					GameContext ctx = {0};
					setup_game_context(
						&ctx, game_state.level, GF_TRY_SINGLE | GF_REPLAY | GF_TRY_KEVIN);
					ctx.num_players = num_players;
					for (PlayerID i = 0; i < num_players; i++) {
						ctx.players[i].lives = game_state.players[i].lives;
						ctx.players[i].coins = game_state.players[i].coins;
						ctx.players[i].score = game_state.players[i].score;
					}
					ctx.checkpoint = game_state.checkpoint;

					nuke_game();
					start_game(&ctx);
					goto break_events;
				}

			no_warp:
				break;
			}

			default:
				break;
			}
		}

	break_events:
		next_tick();
	}

no_tick:
	ftick = pendingticks();
	for (GameActor* actor = get_actor(game_state.live_actors); actor != NULL; actor = get_actor(actor->previous)) {
		InterpActor* iactor = &interp.actors[actor->id];
		iactor->pos.x
			= (fixed)((float)iactor->from.x + (((float)iactor->to.x - (float)iactor->from.x) * ftick));
		iactor->pos.y
			= (fixed)((float)iactor->from.y + (((float)iactor->to.y - (float)iactor->from.y) * ftick));
	}

	return true;

byebye_game:
	nuke_game();
	disconnect();
	return false;
}

static mat4 proj = GLM_MAT4_IDENTITY;

static void perform_camera_magic() {
	VideoCamera* camera = &video_state.camera;

	const GamePlayer* player = get_player(view_player);
	const GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll) {
		InterpActor* iautoscroll = &interp.actors[autoscroll->id];
		camera->pos[0] = FtInt(iautoscroll->pos.x + F_HALF_SCREEN_WIDTH);
		camera->pos[1] = FtInt(iautoscroll->pos.y + F_HALF_SCREEN_HEIGHT);

		const float bx1 = HALF_SCREEN_WIDTH, by1 = HALF_SCREEN_HEIGHT,
			    bx2 = FtInt(game_state.size.x - F_HALF_SCREEN_WIDTH),
			    by2 = FtInt(game_state.size.y - F_HALF_SCREEN_HEIGHT);
		camera->pos[0] = SDL_clamp(camera->pos[0], bx1, bx2);
		camera->pos[1] = SDL_clamp(camera->pos[1], by1, by2);
	} else if (player) {
		const GameActor* pawn = get_actor(player->actor);
		if (pawn != NULL) {
			InterpActor* ipawn = &interp.actors[pawn->id];
			camera->pos[0] = FtInt(ipawn->pos.x);
			camera->pos[1] = FtInt(ipawn->pos.y);

			const float bx1 = FtInt(player->bounds.start.x + F_HALF_SCREEN_WIDTH),
				    by1 = FtInt(player->bounds.start.y + F_HALF_SCREEN_HEIGHT),
				    bx2 = FtInt(player->bounds.end.x - F_HALF_SCREEN_WIDTH),
				    by2 = FtInt(player->bounds.end.y - F_HALF_SCREEN_HEIGHT);
			camera->pos[0] = SDL_clamp(camera->pos[0], bx1, bx2);
			camera->pos[1] = SDL_clamp(camera->pos[1], by1, by2);
		}
	}

	static vec2 cpos = GLM_VEC2_ZERO;
	if (camera->lerp_time[0] < camera->lerp_time[1]) {
		glm_vec2_lerp(camera->from, camera->pos, camera->lerp_time[0] / camera->lerp_time[1], cpos);
		camera->lerp_time[0] += dt();
	} else {
		glm_vec2_copy(camera->pos, cpos);
	}
	glm_ortho(cpos[0] - HALF_SCREEN_WIDTH, cpos[0] + HALF_SCREEN_WIDTH, cpos[1] - HALF_SCREEN_HEIGHT,
		cpos[1] + HALF_SCREEN_HEIGHT, -16000, 16000, proj);
	set_projection_matrix(proj);
	apply_matrices();
	move_ears(camera->pos);
}

static void draw_hud() {
	GamePlayer* player = get_player(view_player);
	if (!player)
		return;

	glm_ortho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, -16000, 16000, proj);
	set_projection_matrix(proj);
	apply_matrices();

	batch_start(XYZ(32.f, 16.f, -10000.f), 0.f, WHITE);
	batch_string("hud", 16, fmt("MARIO * %u", SDL_max(player->lives, 0)));
	batch_cursor(XYZ(147.f, 34.f, -10000.f));
	batch_align(FA_RIGHT, FA_TOP);
	batch_string("hud", 16, fmt("%u", player->score));

	const char* tex;
	switch ((int)((float)(game_state.time) / 6.25f) % 4) {
	default:
		tex = "ui/coins";
		break;
	case 1:
	case 3:
		tex = "ui/coins2";
		break;
	case 2:
		tex = "ui/coins3";
		break;
	}
	batch_cursor(XYZ(224.f, 34.f, -10000.f));
	batch_sprite(tex, NO_FLIP);
	batch_cursor(XYZ(288.f, 34.f, -10000.f));
	batch_string("hud", 16, fmt("%u", player->coins));

	batch_cursor(XYZ(432.f, 16.f, -10000.f));
	batch_sprite(game_state.world, NO_FLIP);

	if (game_state.clock >= 0L) {
		GLfloat scale;
		if ((game_state.flags & GF_HURRY) && video_state.hurry < 120.f) {
			video_state.hurry += dt();
			if (video_state.hurry < 8.f)
				scale = 1.f + ((video_state.hurry / 8.f) * 0.6f);
			else if (video_state.hurry >= 8.f && video_state.hurry <= 112.f)
				scale = 1.6f;
			else if (video_state.hurry > 112.f)
				scale = 1.6f - (((video_state.hurry - 112.f) / 8.f) * 0.6f);
		} else
			scale = 1.f;

		const GLfloat x = (float)SCREEN_WIDTH - (32.f * scale);
		const GLfloat size = 16 * scale;

		batch_cursor(XYZ(x, 24.f * scale, -10000.f));

		GLfloat yscale;
		if (video_state.hurry > 0.f && video_state.hurry < 120.f)
			yscale = glm_lerp(
				1.f, 0.8f + (SDL_sinf(video_state.hurry * 0.6f) * 0.2f), (scale - 1.0f) / 0.6f);
		else
			yscale = 1.f;
		batch_scale(XY(1.f, yscale));

		batch_align(FA_RIGHT, FA_MIDDLE);
		batch_string("hud", size, "TIME");
		batch_scale(XY(1.f, 1.f));

		batch_cursor(XYZ(x, 34.f * scale, -10000.f));
		batch_align(FA_RIGHT, FA_TOP);
		batch_string("hud", size, fmt("%u", game_state.clock));
	}

	if (game_state.sequence.type == SEQ_LOSE && game_state.sequence.time > 0L) {
		batch_cursor(XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, -10000.f));
		batch_align(FA_CENTER, FA_MIDDLE);
		batch_string("hud", 16, (game_state.clock == 0) ? "TIME UP" : "GAME OVER");
	} else if (local_player != view_player) {
		batch_cursor(XYZ(32, 64, -10000.f));
		batch_color(ALPHA(160));
		batch_align(FA_LEFT, FA_TOP);
		batch_string("main", 24, fmt("Spectating: %s", get_peer_name(view_player)));
	}
}

void draw_game() {
	if (game_session == NULL)
		return;

	start_drawing();
	batch_start(ORIGO, 0, WHITE);
	batch_sprite("ui/sidebar_l", NO_FLIP);
	batch_sprite("ui/sidebar_r", NO_FLIP);

	push_surface(game_surface);

	perform_camera_magic();
	// clear_color(0.f, 0.f, 0.f, 1.f);
	clear_depth(1.f);

	draw_tilemaps();

	const GameActor* actor = get_actor(game_state.live_actors);
	while (actor != NULL) {
		if (ANY_FLAG(actor, FLG_VISIBLE))
			ACTOR_CALL(actor, draw);
		actor = get_actor(actor->previous);
	}

	draw_hud();
	draw_chat_hist();
	draw_chat_message();
	pop_surface();

	batch_start(ORIGO, 0, WHITE);
	batch_surface(game_surface);

	stop_drawing();
}

static void destroy_actor(GameActor*);
void tick_game_state(const GameInput inputs[MAX_PLAYERS]) {
	for (PlayerID i = 0; i < num_players; i++) {
		GamePlayer* player = get_player(i);
		player->last_input = player->input;
		player->input = inputs[i];
	}

	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL) {
		game_state.bounds.start.x = Fclamp(autoscroll->pos.x, FxZero, game_state.size.x - F_SCREEN_WIDTH);
		game_state.bounds.start.y = Fclamp(autoscroll->pos.y, FxZero, game_state.size.y - F_SCREEN_HEIGHT);
		game_state.bounds.end.x = Fclamp(autoscroll->pos.x + F_SCREEN_WIDTH, F_SCREEN_WIDTH, game_state.size.x);
		game_state.bounds.end.y
			= Fclamp(autoscroll->pos.y + F_SCREEN_HEIGHT, F_SCREEN_HEIGHT, game_state.size.y);
	}

	if (game_state.pswitch > 0L) {
		--game_state.pswitch;

		if (game_state.pswitch == 100L)
			play_state_sound("starman");

		if (game_state.pswitch <= 0L) {
			replace_actors(ACT_PSWITCH_BRICK, ACT_COIN);
			replace_actors(ACT_PSWITCH_COIN, ACT_BRICK_BLOCK);
			stop_state_track(TS_EVENT);
		}
	}

	GameActor* actor = get_actor(game_state.live_actors);
	while (actor != NULL) {
		if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE)) {
			if (VAL(actor, VAL_SPROUT) > 0L)
				--VAL(actor, VAL_SPROUT);
			else
				ACTOR_CALL(actor, tick);
		}

		GameActor* next = get_actor(actor->previous);
		if (ANY_FLAG(actor, FLG_DESTROY))
			destroy_actor(actor);
		actor = next;
	}

	++game_state.time;
	switch (game_state.sequence.type) {
	default:
		break;

	case SEQ_NONE: {
		if (game_state.clock <= 0L || (game_state.time % 25L) != 0L)
			break;

		--game_state.clock;
		if (game_state.clock <= 100L && !(game_state.flags & GF_HURRY)) {
			play_state_sound("hurry");
			game_state.flags |= GF_HURRY;
		}

		if (game_state.clock <= 0L)
			for (PlayerID i = 0; i < num_players; i++) {
				GamePlayer* player = get_player(i);
				if (player == NULL)
					continue;
				kill_player(get_actor(player->actor));
			}

		break;
	}

	case SEQ_LOSE: {
		if (game_state.sequence.time <= 0L || game_state.sequence.time > 300L)
			break;
		if (++game_state.sequence.time <= 300L)
			break;
		SDL_memset(game_state.next, 0, sizeof(game_state.next));
		game_state.flags |= GF_END;
		break;
	}

	case SEQ_WIN: {
		const uint16_t duration = (game_state.flags & GF_LOST) ? 150L : 400L;
		++game_state.sequence.time;

		if (game_state.sequence.time > duration && game_state.clock > 0L) {
			const int32_t diff = (game_state.clock > 0L) + (((game_state.clock - 1L) >= 10L) * 10L);
			game_state.clock -= diff;

			GamePlayer* player = get_player(game_state.sequence.activator);
			if (player != NULL)
				player->score += diff * 10L;

			if ((game_state.time % 5L) == 0L)
				play_state_sound("tick");
			--game_state.sequence.time;
		}

		if (game_state.sequence.time > (duration + 50L))
			game_state.flags |= GF_END;
	}

	case SEQ_AMBUSH: {
		if (game_state.sequence.time <= 0L)
			game_state.sequence.type = SEQ_AMBUSH_END;
		break;
	}

	case SEQ_AMBUSH_END: {
		if (++game_state.sequence.time < 50L)
			break;

		GamePlayer* winner = get_player(game_state.sequence.activator);
		if (winner != NULL && get_actor(winner->actor) != NULL)
			goto youre_winner;

		// Compensate with lowest scoring player
		winner = NULL;
		uint32_t score = 4294967295L; // World's most paranoid coder award
		for (PlayerID i = 0; i < num_players; i++) {
			GamePlayer* loser = get_player(i);
			if (loser == NULL || loser->score > score || get_actor(loser->actor) == NULL)
				continue;
			winner = loser;
			score = loser->score;
		}
		if (winner == NULL)
			break;

	youre_winner:
		win_player(get_actor(winner->actor));
		break;
	}

	break;
	}
}

void save_game_state(GameState* gs) {
	SDL_memcpy(gs, &game_state, sizeof(GameState));
}

void load_game_state(const GameState* gs) {
	SDL_memcpy(&game_state, gs, sizeof(GameState));
}

uint32_t check_game_state() {
	uint32_t checksum = 0;
	const uint8_t* data = (uint8_t*)(&game_state);
	for (uint32_t i = 0; i < sizeof(GameState); i++)
		checksum += data[i];
	return checksum;
}

void dump_game_state() {}

void nuke_game_state() {
	clear_tilemaps();
	SDL_memset(&game_state, 0, sizeof(game_state));

	for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
		game_state.players[i].id = NULLPLAY;
		game_state.players[i].actor = NULLACT;
		for (ActorID j = 0; j < MAX_MISSILES; j++)
			game_state.players[i].missiles[j] = NULLACT;
		for (ActorID j = 0; j < MAX_SINK; j++)
			game_state.players[i].sink[j] = NULLACT;
		game_state.players[i].kevin.actor = NULLACT;
	}

	game_state.live_actors = NULLACT;
	for (ActorID i = 0; i < MAX_ACTORS; i++) {
		game_state.actors[i].id = game_state.actors[i].previous = game_state.actors[i].next
			= game_state.actors[i].previous_cell = game_state.actors[i].next_cell = NULLACT;
	}
	for (int32_t i = 0; i < GRID_SIZE; i++)
		game_state.grid[i] = NULLACT;

	game_state.size.x = game_state.bounds.end.x = F_SCREEN_WIDTH;
	game_state.size.y = game_state.bounds.end.y = F_SCREEN_HEIGHT;

	game_state.spawn = game_state.checkpoint = game_state.autoscroll = NULLACT;
	game_state.water = FfInt(32767L);

	game_state.clock = -1L;

	game_state.sequence.activator = NULLPLAY;

	SDL_memset(&interp, 0, sizeof(interp));
}

static void read_string(const char** buf, char* dest, uint32_t maxlen) {
	uint32_t i = 0;
	while (**buf != '\0') {
		if (i < maxlen)
			dest[i++] = **buf;
		*buf += 1;
	}
	*buf += 1;
	dest[SDL_min(i, maxlen - 1)] = '\0';
}

#define FLOAT_OFFS(idx) (*((GLfloat*)(buf + sizeof(GLfloat[(idx)]))))
#define BYTE_OFFS(idx) (*((GLubyte*)(buf + sizeof(GLubyte[(idx)]))))
void start_game_state(GameContext* ctx) {
	nuke_game_state();

	if (num_players <= 0 || num_players > MAX_PLAYERS)
		FATAL("Invalid player count for game! Expected 1 to 4, got %i", num_players);
	for (PlayerID i = 0; i < num_players; i++)
		game_state.players[i].id = i;

	SDL_strlcpy(game_state.level, ctx->level, sizeof(game_state.level));
	game_state.flags |= ctx->flags;
	game_state.checkpoint = ctx->checkpoint;

	//
	//
	//
	// DATA LOADER
	//
	//
	//
	load_texture("ui/coins");
	load_texture("ui/coins2");
	load_texture("ui/coins3");
	load_texture("markers/water");
	load_texture("markers/water2");
	load_texture("markers/water3");
	load_texture("markers/water4");
	load_texture("markers/water5");
	load_texture("markers/water6");
	load_texture("markers/water7");
	load_texture("markers/water8");

	load_font("hud");

	load_sound("hurry");
	load_sound("tick");

	//
	//
	//
	// LEVEL LOADER
	//
	//
	//
	uint8_t* data = SDL_LoadFile(find_data_file(fmt("data/levels/%s.*", ctx->level), NULL), NULL);
	ASSUME(data, "Failed to load level \"%s\": %s", ctx->level, SDL_GetError());
	const uint8_t* buf = data;

	// Header
	if (SDL_strncmp((const char*)buf, "Klawiatura", 10) != 0) {
		WTF("Invalid level header");
		SDL_free(data);
		return;
	}
	buf += 10;

	const uint8_t major = *buf;
	if (major != MAJOR_LEVEL_VERSION) {
		WTF("Invalid major version (%u != %u)", major, MAJOR_LEVEL_VERSION);
		SDL_free(data);
		return;
	}
	buf++;

	const uint8_t minor = *buf;
	if (minor != MINOR_LEVEL_VERSION) {
		WTF("Invalid minor version (%u != %u)", minor, MINOR_LEVEL_VERSION);
		SDL_free(data);
		return;
	}
	buf++;

	// Level
	char level_name[32];
	SDL_memcpy(level_name, buf, 32);
	INFO("Level: %s (%s)", ctx->level, level_name);
	buf += 32;

	read_string((const char**)(&buf), game_state.world, sizeof(game_state.world));
	load_texture(game_state.world);

	read_string((const char**)(&buf), game_state.next, sizeof(game_state.next));

	char track_name[2][GAME_STRING_MAX];
	for (int i = 0; i < 2; i++) {
		read_string((const char**)(&buf), track_name[i], sizeof(track_name));
		load_track(track_name[i]);
	}

	game_state.flags |= *((GameFlag*)buf);
	buf += sizeof(GameFlag);
	if (game_state.flags & GF_AMBUSH)
		game_state.sequence.type = SEQ_AMBUSH;

	game_state.size.x = *((fixed*)buf);
	buf += sizeof(fixed);
	game_state.size.y = *((fixed*)buf);
	buf += sizeof(fixed);

	game_state.bounds.start.x = *((fixed*)buf);
	buf += sizeof(fixed);
	game_state.bounds.start.y = *((fixed*)buf);
	buf += sizeof(fixed);
	game_state.bounds.end.x = *((fixed*)buf);
	buf += sizeof(fixed);
	game_state.bounds.end.y = *((fixed*)buf);
	buf += sizeof(fixed);

	game_state.clock = *((int32_t*)buf);
	buf += sizeof(int32_t);

	game_state.water = *((fixed*)buf);
	buf += sizeof(fixed);

	game_state.hazard = *((fixed*)buf);
	buf += sizeof(fixed);

	// Markers
	uint32_t num_markers = *((uint32_t*)buf);
	buf += sizeof(uint32_t);

	for (uint32_t i = 0; i < num_markers; i++) {
		// Skip editor def
		while (*buf != '\0')
			buf++;
		buf++;

		uint8_t marker_type = *((uint8_t*)buf);
		buf++;

		switch (marker_type) {
		default:
			FATAL("Unknown marker type %u", marker_type);
			break;

		case 1: { // Gradient
			char texture_name[GAME_STRING_MAX];
			read_string((const char**)(&buf), texture_name, sizeof(texture_name));

			GLfloat rect[2][2] = {
				{FLOAT_OFFS(0), FLOAT_OFFS(1)},
                                {FLOAT_OFFS(2), FLOAT_OFFS(3)}
                        };
			buf += sizeof(GLfloat[2][2]);

			GLfloat z = *((GLfloat*)buf);
			buf += sizeof(GLfloat);

			GLubyte color[4][4] = {
				{BYTE_OFFS(0),  BYTE_OFFS(1),  BYTE_OFFS(2),  BYTE_OFFS(3) },
				{BYTE_OFFS(4),  BYTE_OFFS(5),  BYTE_OFFS(6),  BYTE_OFFS(7) },
				{BYTE_OFFS(8),  BYTE_OFFS(9),  BYTE_OFFS(10), BYTE_OFFS(11)},
				{BYTE_OFFS(12), BYTE_OFFS(13), BYTE_OFFS(14), BYTE_OFFS(15)}
                        };
			buf += sizeof(GLubyte[4][4]);

			tile_rectangle(texture_name, rect, z, color);
			break;
		}

		case 2: { // Backdrop
			char texture_name[GAME_STRING_MAX];
			read_string((const char**)(&buf), texture_name, sizeof(texture_name));

			GLfloat pos[3] = {FLOAT_OFFS(0), FLOAT_OFFS(1), FLOAT_OFFS(2)};
			buf += sizeof(GLfloat[3]);

			GLfloat scale[2] = {FLOAT_OFFS(0), FLOAT_OFFS(1)};
			buf += sizeof(GLfloat[2]);

			GLubyte color[4] = {BYTE_OFFS(0), BYTE_OFFS(1), BYTE_OFFS(2), BYTE_OFFS(3)};
			buf += sizeof(GLubyte[4]);

			tile_sprite(texture_name, pos, scale, color);
			break;
		}

		case 3: { // Object
			GameActorType type = *((GameActorType*)buf);
			buf += sizeof(GameActorType);

			fvec2 pos = {*((fixed*)buf), *((fixed*)(buf + sizeof(fixed)))};
			buf += sizeof(fvec2);

			GameActor* actor = create_actor(type, pos);
			if (actor == NULL) {
				WARN("Unknown actor type %u", type);

				buf += sizeof(fixed);
				buf += sizeof(fvec2);

				uint8_t num_values = *((uint8_t*)buf);
				buf++;

				for (uint8_t j = 0; j < num_values; j++)
					buf += 1 + sizeof(ActorValue);

				buf += sizeof(ActorFlag);
				break;
			}

			actor->depth = *((fixed*)buf);
			buf += sizeof(fixed);

			fvec2 scale = {*((fixed*)buf), *((fixed*)(buf + sizeof(fixed)))};
			buf += sizeof(fvec2);
			actor->box.start.x = Fmul(actor->box.start.x, scale.x);
			actor->box.start.y = Fmul(actor->box.start.y, scale.y);
			actor->box.end.x = Fmul(actor->box.end.x, scale.x);
			actor->box.end.y = Fmul(actor->box.end.y, scale.y);

			uint8_t num_values = *((uint8_t*)buf);
			buf++;

			for (uint8_t j = 0; j < num_values; j++) {
				uint8_t idx = *((uint8_t*)buf);
				buf++;

				VAL(actor, idx) = *((ActorValue*)buf);
				buf += sizeof(ActorValue);
			}

			FLAG_ON(actor, *((ActorFlag*)buf));
			buf += sizeof(ActorFlag);

			if (actor->type == ACT_HIDDEN_BLOCK && ANY_FLAG(actor, FLG_BLOCK_ONCE)
				&& (game_state.flags & GF_REPLAY))
				FLAG_ON(actor, FLG_DESTROY);
			break;
		}
		}
	}

	SDL_free(data);

	for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;

		player->lives = ctx->players[i].lives;
		player->coins = ctx->players[i].coins;
		player->score = ctx->players[i].score;
		player->power = ctx->players[i].power;
		respawn_player(player);
	}

	play_state_track(TS_MAIN, track_name[0], PLAY_LOOPING);
}
#undef FLOAT_OFFS
#undef BYTE_OFFS

// =======
// PLAYERS
// =======

/// Fetch a player by its `PlayerID`.
GamePlayer* get_player(PlayerID id) {
	if (id < 0L || id >= num_players)
		return NULL;
	GamePlayer* player = &game_state.players[id];
	return (player->id == NULLPLAY) ? NULL : player;
}

/// Attempts to respawn the player.
GameActor* respawn_player(GamePlayer* player) {
	if (player == NULL || game_state.sequence.type == SEQ_WIN)
		return NULL;

	if (player->lives < 0L) {
		// !!! CLIENT-SIDE !!!
		if (view_player == player->id)
			for (PlayerID i = 0; i < num_players; i++) {
				GamePlayer* p = get_player(i);
				if (p != NULL && p->lives >= 0L) {
					set_view_player(p);
					break;
				}
			}
		// !!! CLIENT-SIDE !!!

		return NULL;
	}

	GameActor* pawn = get_actor(player->actor);
	if (pawn != NULL)
		FLAG_ON(pawn, FLG_DESTROY);

	GameActor* spawn = get_actor(game_state.autoscroll);
	if (spawn == NULL)
		spawn = get_actor(game_state.checkpoint);
	if (spawn == NULL)
		spawn = get_actor(game_state.spawn);
	if (spawn == NULL)
		return NULL;

	if (spawn->type == ACT_CHECKPOINT
		&& VAL(spawn, VAL_CHECKPOINT_BOUNDS_X1) != VAL(spawn, VAL_CHECKPOINT_BOUNDS_X2)
		&& VAL(spawn, VAL_CHECKPOINT_BOUNDS_Y1) != VAL(spawn, VAL_CHECKPOINT_BOUNDS_Y2))
	{
		player->bounds.start.x = VAL(spawn, VAL_CHECKPOINT_BOUNDS_X1),
		player->bounds.start.y = VAL(spawn, VAL_CHECKPOINT_BOUNDS_Y1),
		player->bounds.end.x = VAL(spawn, VAL_CHECKPOINT_BOUNDS_X2),
		player->bounds.end.y = VAL(spawn, VAL_CHECKPOINT_BOUNDS_Y2);
	} else
		SDL_memcpy(&player->bounds, &game_state.bounds, sizeof(game_state.bounds));

	pawn = create_actor(
		ACT_PLAYER, (spawn->type == ACT_AUTOSCROLL)
				    ? (fvec2){Flerp(game_state.bounds.start.x, game_state.bounds.end.x, FxHalf),
					      game_state.bounds.start.y}
				    : spawn->pos);
	if (pawn != NULL) {
		VAL(pawn, VAL_PLAYER_INDEX) = (ActorValue)player->id;
		player->actor = pawn->id;
	}
	return pawn;
}

/// Gets the nearest player actor from the position.
GameActor* nearest_pawn(const fvec2 pos) {
	GameActor* nearest = NULL;
	fixed dist = FxZero;

	for (PlayerID i = 0; i < num_players; i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;
		GameActor* pawn = get_actor(player->actor);
		if (pawn == NULL)
			continue;

		const fixed ndist = Vdist(pos, pawn->pos);
		if (nearest == NULL || ndist < dist) {
			nearest = pawn;
			dist = ndist;
		}
	}

	return nearest;
}

/// Gets the ID of the player that the actor belongs to.
PlayerID get_owner_id(const GameActor* actor) {
	return (PlayerID)((ACTORS[actor->type] == NULL || ACTORS[actor->type]->owner == NULL)
				  ? NULLPLAY
				  : ACTORS[actor->type]->owner(actor));
}

/// Gets the player that the actor belongs to.
GamePlayer* get_owner(const GameActor* actor) {
	return get_player(get_owner_id(actor));
}

PlayerID localplayer() {
	return local_player;
}

PlayerID viewplayer() {
	return view_player;
}

PlayerID numplayers() {
	return num_players;
}

void set_view_player(GamePlayer* player) {
	view_player = (PlayerID)((player == NULL) ? NULLPLAY : player->id);
}

// ======
// ACTORS
// ======

/// Load an actor.
void load_actor(GameActorType type) {
	if (type <= ACT_NULL || type >= ACT_SIZE)
		INFO("Loading invalid actor %u", type);
	else
		ACTOR_TYPE_CALL(type, load);
}

/// Create an actor.
GameActor* create_actor(GameActorType type, const fvec2 pos) {
	if (type <= ACT_NULL || type >= ACT_SIZE) {
		WARN("Creating invalid actor %u", type);
		return NULL;
	}

	ActorID index = game_state.next_actor;
	GameActor* actor = NULL;
	for (ActorID i = 0L; i < MAX_ACTORS; i++) {
		actor = &game_state.actors[index];
		if (actor->id == NULLACT)
			goto found;
		index = (ActorID)((index + 1L) % MAX_ACTORS);
	}

	WARN("Too many actors!!!");
	return NULL;

found:
	load_actor(type);
	SDL_memset(actor, 0, sizeof(GameActor));

	actor->id = index;
	actor->type = type;

	actor->previous = game_state.live_actors;
	actor->next = NULLACT;
	GameActor* first = get_actor(game_state.live_actors);
	if (first != NULL)
		first->next = index;
	game_state.live_actors = index;

	actor->cell = NULLCELL;
	actor->previous_cell = actor->next_cell = NULLACT;
	move_actor(actor, pos);

	InterpActor* iactor = &interp.actors[index];
	iactor->type = type;
	iactor->from.x = iactor->to.x = iactor->pos.x = pos.x;
	iactor->from.y = iactor->to.y = iactor->pos.y = pos.y;

	FLAG_ON(actor, FLG_VISIBLE);
	ACTOR_CALL(actor, create);

	game_state.next_actor = (ActorID)((index + 1L) % MAX_ACTORS);
	return actor;
}

/// (INTERNAL) Nuke an actor.
///
/// Do not use this function directly. Use the `FLG_DESTROY` flag on actors instead.
static void destroy_actor(GameActor* actor) {
	if (actor == NULL)
		return;

	const GameActorType type = actor->type;
	if (type <= ACT_NULL || type >= ACT_SIZE)
		WARN("Destroying invalid actor %u", type);
	else
		ACTOR_CALL(actor, cleanup);

	// Unlink cell
	if (actor->cell >= 0L && actor->cell < GRID_SIZE) {
		GameActor* neighbor = get_actor(actor->previous_cell);
		if (neighbor != NULL)
			neighbor->next_cell = actor->next_cell;

		neighbor = get_actor(actor->next_cell);
		if (neighbor != NULL)
			neighbor->previous_cell = actor->previous_cell;

		if (game_state.grid[actor->cell] == actor->id)
			game_state.grid[actor->cell] = actor->previous_cell;

		actor->previous_cell = actor->next_cell = NULLACT;
		actor->cell = NULLCELL;
	}

	// Unlink list
	GameActor* neighbor = get_actor(actor->previous);
	if (neighbor != NULL)
		neighbor->next = actor->next;

	neighbor = get_actor(actor->next);
	if (neighbor != NULL)
		neighbor->previous = actor->previous;

	if (game_state.live_actors == actor->id)
		game_state.live_actors = actor->previous;

	actor->previous = actor->next = NULLACT;

	actor->id = NULLACT;
	actor->type = ACT_NULL;
}

/// Destroys all `before` actors and creates `after` actors on top of them.
void replace_actors(GameActorType before, GameActorType after) {
	for (GameActor* actor = get_actor(game_state.live_actors); actor != NULL; actor = get_actor(actor->previous)) {
		if (actor->type != before)
			continue;
		ACTOR_CALL(actor, cleanup);
		actor->type = after;
		ACTOR_CALL(actor, create);
	}
}

/// Fetch an actor by its `ActorID`.
GameActor* get_actor(ActorID id) {
	if (id < 0L || id >= MAX_ACTORS)
		return NULL;
	GameActor* actor = &game_state.actors[id];
	return (actor->id == NULLACT) ? NULL : actor;
}

/// Move an actor and determine whether or not it actually moved.
void move_actor(GameActor* actor, const fvec2 pos) {
	if (actor == NULL)
		return;

	actor->pos = pos;
	int32_t cx = actor->pos.x / CELL_SIZE, cy = actor->pos.y / CELL_SIZE;
	cx = SDL_clamp(cx, 0L, MAX_CELLS - 1L), cy = SDL_clamp(cy, 0L, MAX_CELLS - 1L);

	const int32_t cell = cx + (cy * MAX_CELLS);
	if (cell == actor->cell)
		return;

	// Unlink old cell
	if (actor->cell >= 0L && actor->cell < GRID_SIZE) {
		GameActor* neighbor = get_actor(actor->previous_cell);
		if (neighbor != NULL)
			neighbor->next_cell = actor->next_cell;

		neighbor = get_actor(actor->next_cell);
		if (neighbor != NULL)
			neighbor->previous_cell = actor->previous_cell;

		if (game_state.grid[actor->cell] == actor->id)
			game_state.grid[actor->cell] = actor->previous_cell;
	}

	// Link new cell
	actor->cell = cell;
	actor->previous_cell = game_state.grid[cell];
	actor->next_cell = NULLACT;
	GameActor* first = get_actor(game_state.grid[cell]);
	if (first != NULL)
		first->next_cell = actor->id;
	game_state.grid[cell] = actor->id;
}

/// Check if the actor's hitbox is below the level.
Bool below_level(GameActor* actor) {
	return actor != NULL && (actor->pos.y + actor->box.start.y) > game_state.size.y;
}

/// Check if the actor is within any player's range.
#define CHECK_AUTOSCROLL_VIEW                                                                                          \
	const GameActor* autoscroll = get_actor(game_state.autoscroll);                                                \
	if (autoscroll != NULL) {                                                                                      \
		const frect cbox = {Vadd(game_state.bounds.start, (fvec2){padding, padding}),                          \
			Vadd(game_state.bounds.end, (fvec2){padding, padding})};                                       \
		return (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y            \
			&& (ignore_top || abox.end.y > cbox.start.y));                                                 \
	}

Bool in_any_view(GameActor* actor, fixed padding, Bool ignore_top) {
	if (actor == NULL)
		return false;

	const frect abox = HITBOX(actor);

	CHECK_AUTOSCROLL_VIEW;

	for (PlayerID i = 0; i < num_players; i++) {
		const GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;

		const fixed cx = Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
				    player->bounds.end.x - F_HALF_SCREEN_WIDTH),
			    cy = Fclamp(player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT,
				    player->bounds.end.y - F_HALF_SCREEN_HEIGHT);

		const frect cbox = {
			{cx - F_HALF_SCREEN_WIDTH - padding, cy - F_HALF_SCREEN_HEIGHT - padding},
			{cx + F_HALF_SCREEN_WIDTH + padding, cy + F_HALF_SCREEN_HEIGHT + padding}
                };
		if (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y
			&& (ignore_top || abox.end.y > cbox.start.y))
			return true;
	}

	return false;
}

/// Check if the actor is within a player's range.
Bool in_player_view(GameActor* actor, GamePlayer* player, fixed padding, Bool ignore_top) {
	if (actor == NULL || player == NULL)
		return false;

	const frect abox = HITBOX(actor);

	CHECK_AUTOSCROLL_VIEW;

	const fixed cx = Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
			    player->bounds.end.x - F_HALF_SCREEN_WIDTH),
		    cy = Fclamp(player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT,
			    player->bounds.end.y - F_HALF_SCREEN_HEIGHT);

	const frect cbox = {
		{cx - F_HALF_SCREEN_WIDTH - padding, cy - F_HALF_SCREEN_HEIGHT - padding},
		{cx + F_HALF_SCREEN_WIDTH + padding, cy + F_HALF_SCREEN_HEIGHT + padding}
        };
	return (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y
		&& (ignore_top || abox.end.y > cbox.start.y));
}

#undef CHECK_AUTOSCROLL_VIEW

/// Retrieve a list of actors overlapping a rectangle.
void list_cell_at(CellList* list, const frect rect) {
	list->num_actors = 0L;

	int32_t cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE;
	int32_t cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
	int32_t cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE;
	int32_t cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L);
	cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L);
	cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (int32_t cx = cx1; cx <= cx2; cx++)
		for (int32_t cy = cy1; cy <= cy2; cy++)
			for (GameActor* actor = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]); actor != NULL;
				actor = get_actor(actor->previous_cell))
				if (!ANY_FLAG(actor, FLG_DESTROY) && Rcollide(rect, HITBOX(actor)))
					list->actors[list->num_actors++] = actor;
}

/// Check collision with other actors.
void collide_actor(GameActor* actor) {
	if (actor == NULL)
		return;

	const frect abox = HITBOX(actor);
	int32_t cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE;
	int32_t cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
	int32_t cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE;
	int32_t cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L);
	cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L);
	cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (int32_t cx = cx1; cx <= cx2; cx++)
		for (int32_t cy = cy1; cy <= cy2; cy++)
			for (GameActor* other = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]); other != NULL;
				other = get_actor(other->previous_cell))
			{
				if (actor == other || ANY_FLAG(other, FLG_DESTROY) || !Rcollide(abox, HITBOX(other)))
					continue;
				ACTOR_CALL2(other, collide, actor);
				if (ANY_FLAG(actor, FLG_DESTROY))
					return;
			}
}

/// Check if there are solid actors in a rectangle.
Bool touching_solid(const frect rect, SolidType types) {
	int32_t cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE;
	int32_t cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
	int32_t cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE;
	int32_t cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L);
	cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L);
	cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (int32_t cx = cx1; cx <= cx2; cx++)
		for (int32_t cy = cy1; cy <= cy2; cy++)
			for (GameActor* actor = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]); actor != NULL;
				actor = get_actor(actor->previous_cell))
				if (ACTOR_IS_SOLID(actor, types) && Rcollide(rect, HITBOX(actor)))
					return true;

	return false;
}

/// Move the actor with speed and displacement from solid actors.
void displace_actor(GameActor* actor, fixed climb, Bool unstuck) {
	if (actor == NULL)
		return;

	if (unstuck
		&& touching_solid(
			(frect){
				{actor->pos.x + actor->box.start.x, actor->pos.y + actor->box.start.y      },
				{actor->pos.x + actor->box.end.x,   actor->pos.y + actor->box.end.y - FxOne}
        },
			SOL_SOLID))
	{
		fixed shift = ANY_FLAG(actor, FLG_X_FLIP) ? FxOne : -FxOne;
		if (ANY_FLAG(actor, FLG_X_FLIP)) {
			if (touching_solid(HITBOX_RIGHT(actor), SOL_SOLID)
				&& !touching_solid(HITBOX_LEFT(actor), SOL_SOLID))
				shift = -shift;
		} else {
			if (touching_solid(HITBOX_LEFT(actor), SOL_SOLID)
				&& !touching_solid(HITBOX_RIGHT(actor), SOL_SOLID))
				shift = -shift;
		}

		move_actor(actor, POS_ADD(actor, shift, FxZero));
		VAL(actor, VAL_X_SPEED) = VAL(actor, VAL_Y_SPEED) = FxZero;
		VAL(actor, VAL_X_TOUCH) = ((shift >= FxZero) ? -1L : 1L), VAL(actor, VAL_Y_TOUCH) = 1L;
		return;
	}

	VAL(actor, VAL_X_TOUCH) = VAL(actor, VAL_Y_TOUCH) = 0L;
	fixed x = actor->pos.x, y = actor->pos.y;

	// Horizontal collision
	x += VAL(actor, VAL_X_SPEED);
	CellList list = {0};
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });
	Bool climbed = false;

	if (list.num_actors > 0L) {
		Bool stop = false;
		if (VAL(actor, VAL_X_SPEED) < FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				if (VAL(actor, VAL_Y_SPEED) >= FxZero
					&& (actor->pos.y + actor->box.end.y - climb)
						   < (displacer->pos.y + displacer->box.start.y))
				{
					const fixed step = displacer->pos.y + displacer->box.start.y - actor->box.end.y;
					if (!touching_solid(
						    (frect){
							    {x + actor->box.start.x - FxOne,
                                                             step + actor->box.start.y - FxOne},
							    {x + actor->box.end.x - FxOne,
                                                             step + actor->box.end.y - FxOne  }
                                        },
						    SOL_SOLID))
					{
						y = step;
						VAL(actor, VAL_Y_SPEED) = FxZero;
						VAL(actor, VAL_Y_TOUCH) = 1L;
						climbed = true;
					}
					continue;
				}

				ACTOR_CALL2(displacer, on_right, actor);
				x = Fmax(x, displacer->pos.x + displacer->box.end.x - actor->box.start.x);
				stop = VAL(actor, VAL_X_SPEED) <= FxZero;
				climbed = false;
			}
			VAL(actor, VAL_X_TOUCH) = -(stop && !climbed);
		} else if (VAL(actor, VAL_X_SPEED) > FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				if (VAL(actor, VAL_Y_SPEED) >= FxZero
					&& (actor->pos.y + actor->box.end.y - climb)
						   < (displacer->pos.y + displacer->box.start.y))
				{
					const fixed step = displacer->pos.y + displacer->box.start.y - actor->box.end.y;
					if (!touching_solid(
						    (frect){
							    {x + actor->box.start.x + FxOne,
                                                             step + actor->box.start.y - FxOne},
							    {x + actor->box.end.x + FxOne,
                                                             step + actor->box.end.y - FxOne  }
                                        },
						    SOL_SOLID))
					{
						y = step;
						VAL(actor, VAL_Y_SPEED) = FxZero;
						VAL(actor, VAL_Y_TOUCH) = 1L;
						climbed = true;
					}
					continue;
				}

				ACTOR_CALL2(displacer, on_left, actor);
				x = Fmin(x, displacer->pos.x + displacer->box.start.x - actor->box.end.x);
				stop = VAL(actor, VAL_X_SPEED) >= FxZero;
				climbed = false;
			}
			VAL(actor, VAL_X_TOUCH) = stop && !climbed;
		}

		if (stop)
			VAL(actor, VAL_X_SPEED) = FxZero;
	}

	// Vertical collision
	y += VAL(actor, VAL_Y_SPEED);
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		Bool stop = false;
		if (VAL(actor, VAL_Y_SPEED) < FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_bottom, actor);
				y = Fmax(y, displacer->pos.y + displacer->box.end.y - actor->box.start.y);
				stop = VAL(actor, VAL_Y_SPEED) <= FxZero;
			}
			VAL(actor, VAL_Y_TOUCH) = -(fixed)stop;
		} else if (VAL(actor, VAL_Y_SPEED) > FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer)
					continue;

				const SolidType solid = ACTOR_GET_SOLID(displacer);
				if (!(solid & SOL_ALL)
					|| ((solid & SOL_TOP)
						&& (y + actor->box.end.y - VAL(actor, VAL_Y_SPEED))
							   > (displacer->pos.y + displacer->box.start.y + climb)))
					continue;

				ACTOR_CALL2(displacer, on_top, actor);
				y = Fmin(y, displacer->pos.y + displacer->box.start.y - actor->box.end.y);
				stop = VAL(actor, VAL_Y_SPEED) >= FxZero;
			}
			VAL(actor, VAL_Y_TOUCH) = (fixed)stop;
		}

		if (stop)
			VAL(actor, VAL_Y_SPEED) = FxZero;
	}

	move_actor(actor, (fvec2){x, y});
}

// Generic interpolated (not yet) actor drawing function.
//
// Formula for current static actor frame: `(game_state.time / ((TICKRATE * 2) / speed)) % frames`
void draw_actor(const GameActor* actor, const char* name, GLfloat angle, const GLubyte color[4]) {
	const InterpActor* iactor = &interp.actors[actor->id];
	const ActorValue sprout = VAL(actor, VAL_SPROUT);
	batch_start(
		XYZ(FtInt(iactor->pos.x), FtInt(iactor->pos.y) + sprout, (sprout > 0L) ? 21.f : FtFloat(actor->depth)),
		angle, color);
	batch_sprite(name, FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
}

// Convenience function for playing a sound at an actor's position.
void play_actor_sound(const GameActor* actor, const char* name) {
	play_state_sound_at(name, (float[2]){FtFloat(actor->pos.x), FtFloat(actor->pos.y)});
}

// ====
// MATH
// ====

/// Produce an exclusive random number.
int32_t rng(int32_t x) {
	// https://rosettacode.org/wiki/Linear_congruential_generator
	game_state.seed = (game_state.seed * 1103515245L + 12345L) & 2147483647L;
	return game_state.seed % x;
}

// ======
// INTERP
// ======

#define BAD_ACTOR(actor) ((actor) == NULL || (actor)->id < 0L || (actor)->id >= MAX_ACTORS)

/// Fetch an actor's interpolation data.
const InterpActor* get_interp(const GameActor* actor) {
	if (BAD_ACTOR(actor))
		return NULL;
	return &interp.actors[actor->id];
}

/// Skip interpolating an actor's position.
void skip_interp(const GameActor* actor) {
	if (BAD_ACTOR(actor))
		return;
	InterpActor* iactor = &interp.actors[actor->id];
	iactor->from.x = iactor->to.x = iactor->pos.x = actor->pos.x;
	iactor->from.y = iactor->to.y = iactor->pos.y = actor->pos.y;
}

/// Start interpolating an actor's position from another actor. Used for trails.
void align_interp(const GameActor* actor, const GameActor* from) {
	if (BAD_ACTOR(actor) || BAD_ACTOR(from))
		return;
	InterpActor *iactor = &interp.actors[actor->id], *ifrom = &interp.actors[from->id];
	iactor->from.x = ifrom->pos.x, iactor->from.y = ifrom->pos.y;
}

#undef BAD_ACTOR
