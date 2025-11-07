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
#include "actors/K_checkpoint.h" // IWYU pragma: keep
#include "actors/K_enemies.h"    // IWYU pragma: keep
#include "actors/K_player.h"

#define ACTOR_CALL_STATIC(type, fn)                                                                                    \
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

// Not static since it's `extern` in K_actors.c
const GameActorTable* ACTORS[ACT_SIZE] = {0};

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;
GameState game_state = {0};

PlayerID local_player = NULLPLAY, view_player = NULLPLAY, num_players = 0L;

static InterpState interp = {0};

static Bool paused = false;

// =====
// PAUSE
// =====

static void pause() {
	paused = true;
	if (num_players <= 1L)
		pause_audio_state(true);
}

static void unpause() {
	paused = false;
	pause_audio_state(false);
}

// ====
// CHAT
// ===

#define MAX_CHATS 8

static const GLfloat chat_fs = 18.f;
static char chat_message[100] = {0};
static struct {
	char text[sizeof(chat_message)];
	float lifetime;
} chat_hist[MAX_CHATS] = {0};

static void send_chat_message() {
	if (typing_what() || !SDL_strlen(chat_message))
		return;
	// TODO: use message headers to distinguish message types, instead of using gross hacks such as this.
	static char buf[4 + sizeof(chat_message)] = "CHAT";
	SDL_strlcpy(buf + 4, chat_message, sizeof(chat_message));
	for (PlayerID i = 0; i < num_players; i++)
		NutPunch_SendReliably(player_to_peer(i), buf, sizeof(buf));
	push_chat_message(NutPunch_LocalPeer(), chat_message);
	chat_message[0] = 0;
}

void push_chat_message(const int from, const char* text) {
	for (int i = MAX_CHATS - 1; i >= 1; i--)
		SDL_memcpy(&chat_hist[i], &chat_hist[i - 1], sizeof(*chat_hist));

	const char* line = fmt("%s: %s", get_peer_name(from), text);
	INFO("%s", line);

	SDL_strlcpy(chat_hist[0].text, line, sizeof(chat_message));
	chat_hist[0].lifetime = 6 * TICKRATE;
	play_generic_sound("chat");
}

static void update_chat_hist() {
	const float delta = dt();
	for (int i = 0; i < MAX_CHATS; i++) {
		if (chat_hist[i].lifetime > 0.f)
			chat_hist[i].lifetime -= delta;
		if (chat_hist[i].lifetime <= 0.f) {
			if (i >= (MAX_CHATS - 1))
				SDL_memset(&chat_hist[i], 0, sizeof(*chat_hist));
			else {
				for (int j = i; j < MAX_CHATS - 1; j++)
					SDL_memcpy(&chat_hist[j], &chat_hist[j + 1], sizeof(*chat_hist));
				SDL_memset(&chat_hist[MAX_CHATS - 1], 0, sizeof(*chat_hist));
			}
		}
	}
}

static void draw_chat_hist() {
	const float y = SCREEN_HEIGHT - 12.f - (1.5 * chat_fs);
	const GLubyte a = (typing_what() == chat_message) ? 255 : 192;
	for (int i = 0; i < MAX_CHATS; i++) {
		if (chat_hist[i].lifetime <= 0.f)
			break;
		const float fade = chat_hist[i].lifetime / TICKRATE;
		batch_color(ALPHA(SDL_min(1.f, fade) * a));

		batch_align(FA_LEFT, FA_BOTTOM);
		batch_cursor(XYZ(12.f, y - (i * chat_fs), -10000.f));
		batch_string("main", chat_fs, chat_hist[i].text);
	}
}

static void draw_chat_message() {
	if (typing_what() != chat_message)
		return;

	batch_align(FA_LEFT, FA_BOTTOM);
	batch_cursor(XYZ(12.f, SCREEN_HEIGHT - 12.f, -10000.f));
	batch_color(WHITE);
	batch_string("main", chat_fs, fmt("> %s%s", chat_message, SDL_fmodf(totalticks(), 30.f) < 16.f ? "|" : ""));
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

	unpause();
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

	const float ahh = gekko_frames_ahead(game_session), ahead = SDL_clamp(ahh, 0, 2);
	GameInput input = 0;

	update_chat_hist();
	if (!typing_what() && is_connected()) {
		// FIXME: Make ESC cancel the message instead of sending it.
		if (kb_pressed(KB_CHAT) && !paused)
			start_typing(chat_message, sizeof(chat_message));
		else
			send_chat_message();
	}

	// nonk: make sure to use these three funcs ONLY in a `for` construct. Behavior is undefined otherwise.
	for (new_frame(ahead); got_ticks(); next_tick()) {
		// Capture interpolation data from the START of the current tick, since we're interpolating from the
		// start of one tick to the next.
		for (GameActor* actor = get_actor(game_state.live_actors); actor; actor = get_actor(actor->previous)) {
			InterpActor* iactor = &interp.actors[actor->id];
			if (iactor->type == actor->type)
				iactor->from = actor->pos;
			else {
				// Actor isn't what interp expected, just snap to tick position. Failsafe for rollback.
				iactor->type = actor->type;
				iactor->from = iactor->pos = actor->pos;
			}
		}

		if (paused) {
			if (kb_pressed(KB_UI_ENTER)) {
				play_generic_sound("select");
				goto byebye_game;
			} else if (kb_pressed(KB_PAUSE)) {
				play_generic_sound("select");
				unpause();
			}
		} else if (kb_pressed(KB_PAUSE) && !typing_what()) {
			play_generic_sound("pause");
			pause();
		} else
			goto dont_break_events;

		if (num_players <= 1L)
			continue;

	dont_break_events:
		input = 0; // declaring here spits out a shitass warning...
		if (!paused && !typing_what()) {
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
				show_error("Out of sync with player %i (%s)\n"
					   "Tick: %i\n"
					   "Local Checksum: %u\n"
					   "Remote Checksum: %u\n"
					   "\nGame state has been dumped to console.",
					desync.remote_handle + 1,
					get_peer_name(player_to_peer((PlayerID)desync.remote_handle)), desync.frame,
					desync.local_checksum, desync.remote_checksum);
				goto byebye_game;
			}

			case PlayerConnected: {
				struct Connected connect = event->data.connected;
				INFO("Player %i connected", connect.handle + 1);
				break;
			}

			case PlayerDisconnected: {
				struct Disconnected disconnect = event->data.disconnected;
				show_error("Lost connection to player %i (%s)", disconnect.handle + 1,
					get_peer_name(player_to_peer((PlayerID)disconnect.handle)));
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
						break;

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
						if (lives < 0L && !(ctx.flags & GF_KEVIN))
							continue;
						ctx.players[i].lives = lives;
						ctx.players[i].power = game_state.players[i].power;
						ctx.players[i].coins = game_state.players[i].coins;
						ctx.players[i].score = game_state.players[i].score;
					}

					nuke_game();
					start_game(&ctx);
					return true;
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
					return true;
				}

				break;
			}
			default:
				break;
			}
		}
	}

	// Interpolate outside of the loop for uncapped framerate.
	register const fixed ftick = FfFloat(pendingticks());
	for (const GameActor* actor = get_actor(game_state.live_actors); actor; actor = get_actor(actor->previous)) {
		InterpActor* iactor = &interp.actors[actor->id];
		iactor->pos = Vadd(iactor->from, Vscale(Vsub(actor->pos, iactor->from), ftick));
	}

	return true;

byebye_game:
	nuke_game();
	disconnect();
	return false;
}

static mat4 proj = GLM_MAT4_IDENTITY;
static vec2 camera_offset_morsel = {0.f, 0.f};

static void perform_camera_magic() {
	static vec2 cpos = GLM_VEC2_ZERO;
	VideoCamera* camera = &video_state.camera;
	camera_offset_morsel[0] = camera_offset_morsel[1] = 0.f;

	const GamePlayer* player = get_player(view_player);
	const GameActor* autoscroll = get_actor(game_state.autoscroll);

	if (autoscroll) {
		const InterpActor* iautoscroll = get_interp(autoscroll);
		if (!iautoscroll)
			goto fuck;

		camera->pos[0] = FtInt(iautoscroll->pos.x + F_HALF_SCREEN_WIDTH);
		camera->pos[1] = FtInt(iautoscroll->pos.y + F_HALF_SCREEN_HEIGHT);

		const float bx1 = FtInt(F_HALF_SCREEN_WIDTH), by1 = FtInt(F_HALF_SCREEN_HEIGHT),
			    bx2 = FtInt(game_state.size.x - F_HALF_SCREEN_WIDTH),
			    by2 = FtInt(game_state.size.y - F_HALF_SCREEN_HEIGHT);
		camera->pos[0] = SDL_clamp(camera->pos[0], bx1, bx2);
		camera->pos[1] = SDL_clamp(camera->pos[1], by1, by2);

		camera_offset_morsel[0] = FtFloat(iautoscroll->pos.x) - FtInt(iautoscroll->pos.x);
		camera_offset_morsel[1] = FtFloat(iautoscroll->pos.y) - FtInt(iautoscroll->pos.y);
	} else if (player) {
		const InterpActor* ipawn = get_interp(get_actor(player->actor));
		if (!ipawn)
			goto fuck;

		const float bx1 = FtInt(player->bounds.start.x + F_HALF_SCREEN_WIDTH),
			    by1 = FtInt(player->bounds.start.y + F_HALF_SCREEN_HEIGHT),
			    bx2 = FtInt(player->bounds.end.x - F_HALF_SCREEN_WIDTH),
			    by2 = FtInt(player->bounds.end.y - F_HALF_SCREEN_HEIGHT);

		camera->pos[0] = SDL_clamp(FtInt(ipawn->pos.x), bx1, bx2);
		camera->pos[1] = SDL_clamp(FtInt(ipawn->pos.y), by1, by2);

		camera_offset_morsel[0] = FtFloat(ipawn->pos.x) - FtInt(ipawn->pos.x);
		camera_offset_morsel[1] = FtFloat(ipawn->pos.y) - FtInt(ipawn->pos.y);
	}

fuck:
	if (camera->lerp_time[0] < camera->lerp_time[1]) {
		glm_vec2_lerp(camera->from, camera->pos, camera->lerp_time[0] / camera->lerp_time[1], cpos);
		camera->lerp_time[0] += dt();
	} else {
		glm_vec2_copy(camera->pos, cpos);
	}

	if (video_state.quake > 0.f) {
		const Sint32 quake = (Sint32)video_state.quake + 1L;
		Uint64 seed = game_state.time;
		cpos[0] += (float)(-SDL_rand_r(&seed, quake) + SDL_rand_r(&seed, quake));
		cpos[1] += (float)(-SDL_rand_r(&seed, quake) + SDL_rand_r(&seed, quake));
		video_state.quake -= dt();
	}

	glm_ortho(cpos[0] - HALF_SCREEN_WIDTH, cpos[0] + HALF_SCREEN_WIDTH, cpos[1] - HALF_SCREEN_HEIGHT,
		cpos[1] + HALF_SCREEN_HEIGHT, -16000.f, 16000.f, proj);
	set_projection_matrix(proj);
	apply_matrices();
	move_ears(camera->pos);
}

static void draw_hud() {
	GamePlayer* player = get_player(view_player);
	if (!player)
		return;

	glm_ortho(0.f, SCREEN_WIDTH, 0.f, SCREEN_HEIGHT, -16000.f, 16000.f, proj);
	set_projection_matrix(proj);
	apply_matrices();

	if (paused) {
		batch_start(XYZ(0.f, 0.f, -10000.f), 0.f, RGBA(0, 0, 0, 128));
		batch_rectangle(NULL, XY(SCREEN_WIDTH, SCREEN_HEIGHT));
		batch_cursor(XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, -10000.f)), batch_color(WHITE);
		batch_color(RGB(255, 144, 144)), batch_align(FA_CENTER, FA_BOTTOM);
		batch_string("main", 24, "Paused");
		batch_color(WHITE), batch_align(FA_CENTER, FA_TOP);
		batch_string("main", 24, fmt("[%s] Return to Title", kb_label(KB_UI_ENTER)));
	}

	batch_cursor(XYZ(32.f, 16.f, -10000.f)), batch_color(WHITE), batch_align(FA_LEFT, FA_TOP);
	batch_string("hud", 16, fmt("MARIO * %u", SDL_max(player->lives, 0)));
	batch_cursor(XYZ(147.f, 34.f, -10000.f));
	batch_align(FA_RIGHT, FA_TOP);
	batch_string("hud", 16.f, fmt("%u", player->score));

	const char* tex;
	switch ((int)((float)(game_state.time) / 6.25f) % 4L) {
	default:
		tex = "ui/coins";
		break;
	case 1L:
	case 3L:
		tex = "ui/coins2";
		break;
	case 2L:
		tex = "ui/coins3";
		break;
	}
	batch_cursor(XYZ(224.f, 34.f, -10000.f));
	batch_sprite(tex, NO_FLIP);
	batch_cursor(XYZ(288.f, 34.f, -10000.f));
	batch_string("hud", 16.f, fmt("%u", player->coins));

	batch_cursor(XYZ(432.f, 16.f, -10000.f));
	if (video_state.world[0] == '@') {
		batch_align(FA_CENTER, FA_TOP);
		batch_string("hud", 16.f, video_state.world + 1L);
	} else
		batch_sprite(video_state.world, NO_FLIP);

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
		const GLfloat size = 16.f * scale;

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

	if (!paused && game_state.sequence.type == SEQ_LOSE && game_state.sequence.time > 0L) {
		batch_cursor(XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, -10000.f));
		batch_align(FA_CENTER, FA_MIDDLE);
		batch_string("hud", 16, (game_state.clock == 0) ? "TIME UP" : "GAME OVER");
	} else if (local_player != view_player) {
		batch_cursor(XYZ(32, 64, -10000.f));
		batch_color(ALPHA(160));
		batch_align(FA_LEFT, FA_TOP);
		batch_string("main", 24, fmt("Spectating: %s", get_peer_name(player_to_peer(view_player))));
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

	const char* tex;
	switch ((game_state.time / 5L) % 8L) {
	default:
		tex = "markers/water";
		break;
	case 1L:
		tex = "markers/water2";
		break;
	case 2L:
		tex = "markers/water3";
		break;
	case 3L:
		tex = "markers/water4";
		break;
	case 4L:
		tex = "markers/water5";
		break;
	case 5L:
		tex = "markers/water6";
		break;
	case 6L:
		tex = "markers/water7";
		break;
	case 7L:
		tex = "markers/water8";
		break;
	}

	float wy = FtInt(game_state.water);
	batch_start(XYZ(0.f, wy, -100.f), 0.f, ALPHA(135));
	batch_rectangle(tex, XY(FtFloat(game_state.size.x), 16.f));
	wy += 16.f;
	batch_cursor(XYZ(0.f, wy, -100.f));
	batch_color(RGBA(88, 136, 224, 135));
	batch_rectangle(NULL, XY(FtFloat(game_state.size.x), FtFloat(game_state.size.y) - wy));

	draw_hud();
	pop_surface();

	batch_start(ORIGO, 0, WHITE);
	batch_surface(game_surface);
	draw_chat_hist();
	draw_chat_message();

	stop_drawing();
}

#define TICK_LOOP(ticker)                                                                                              \
	actor = get_actor(game_state.live_actors);                                                                     \
	while (actor != NULL) {                                                                                        \
		if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE) && VAL(actor, SPROUT) <= 0L)                            \
			ACTOR_CALL(actor, ticker);                                                                     \
                                                                                                                       \
		GameActor* next = get_actor(actor->previous);                                                          \
		if (ANY_FLAG(actor, FLG_DESTROY))                                                                      \
			destroy_actor(actor);                                                                          \
		actor = next;                                                                                          \
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
			if (VAL(actor, SPROUT) > 0L)
				--VAL(actor, SPROUT);
			else
				ACTOR_CALL(actor, pre_tick);
		}

		GameActor* next = get_actor(actor->previous);
		if (ANY_FLAG(actor, FLG_DESTROY))
			destroy_actor(actor);
		actor = next;
	}

	TICK_LOOP(tick);
	TICK_LOOP(post_tick);

	++game_state.time;
	switch (game_state.sequence.type) {
	default:
		break;

	case SEQ_AMBUSH: {
		if (game_state.sequence.time <= 0L) {
			game_state.sequence.type = SEQ_AMBUSH_END;
			break;
		}
	}
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
	}
}

#undef TICK_LOOP

void save_game_state(GameState* gs) {
	SDL_memcpy(gs, &game_state, sizeof(GameState));
}

void load_game_state(const GameState* gs) {
	SDL_memcpy(&game_state, gs, sizeof(GameState));

	// Fix for being stuck in spectator during rollback
	if (local_player == view_player)
		return;
	const GamePlayer* player = get_player(local_player);
	if (player != NULL && get_actor(player->actor) != NULL) {
		view_player = player->id;
		video_state.camera.lerp_time[1] = 0.f;
	}
}

uint32_t check_game_state() {
	uint32_t checksum = 0;
	const uint8_t* data = (uint8_t*)(&game_state);
	for (uint32_t i = 0; i < sizeof(GameState); i++)
		checksum += data[i];
	return checksum;
}

void dump_game_state() {
	INFO("====================");
	INFO("Flags: %u", game_state.flags);
	INFO("");
	INFO("[SEQUENCE]");
	INFO("	Type: %u", game_state.sequence.type);
	INFO("	Activator: %i", game_state.sequence.activator);
	INFO("	Time: %u", game_state.sequence.time);
	INFO("");
	INFO("[PLAYERS]");

	for (PlayerID i = 0; i < num_players; i++) {
		const GamePlayer* player = &game_state.players[i];

		INFO("	Player %i:", i + 1);
		INFO("		ID: %i", player->id);
		INFO("		Input: %u -> %u", player->last_input, player->input);
		INFO("		Lives: %i", player->lives);
		INFO("		Coins: %u", player->coins);
		INFO("		Power: %u", player->power);
		INFO("		Actor: %i (%p)", player->actor, get_actor(player->actor));
		INFO("		Missiles:");

		for (ActorID j = 0; j < MAX_MISSILES; j++)
			INFO("			%i (%p)", player->missiles[j], get_actor(player->missiles[j]));

		INFO("		Sinking Missiles:");

		for (ActorID j = 0; j < MAX_SINK; j++)
			INFO("			%i (%p)", player->sink[j], get_actor(player->sink[j]));

		INFO("		Position: (%.2f, %.2f)", FtFloat(player->pos.x), FtFloat(player->pos.y));
		INFO("		Bounds: (%.2f, %.2f, %.2f, %.2f)", FtFloat(player->bounds.start.x),
			FtFloat(player->bounds.start.y), FtFloat(player->bounds.end.x), FtFloat(player->bounds.end.y));
		INFO("		Score: %u", player->score);
		INFO("		Kevin:");
		INFO("			Delay: %i", player->kevin.delay);
		INFO("			Actor: %i (%p)", player->kevin.actor, get_actor(player->kevin.actor));
		INFO("			Start: (%.2f, %.2f)", FtFloat(player->kevin.start.x),
			FtFloat(player->kevin.start.y));
		INFO("			(Frames...)");
		INFO("");
	}

	INFO("");
	INFO("Spawn: %i (%p)", game_state.spawn, get_actor(game_state.spawn));
	INFO("Checkpoint: %i (%p)", game_state.checkpoint, get_actor(game_state.checkpoint));
	INFO("Autoscroll: %i (%p)", game_state.autoscroll, get_actor(game_state.autoscroll));
	INFO("P-Switch: %u", game_state.pswitch);
	INFO("Size: (%.2f, %.2f)", FtFloat(game_state.size.x), FtFloat(game_state.size.y));
	INFO("Bounds: (%.2f, %.2f, %.2f, %.2f)", FtFloat(game_state.bounds.start.x), FtFloat(game_state.bounds.start.y),
		FtFloat(game_state.bounds.end.x), FtFloat(game_state.bounds.end.y));
	INFO("Water Y: %.2f", FtFloat(game_state.water));
	INFO("Hazard Y: %.2f", FtFloat(game_state.hazard));
	INFO("Clock: %i", game_state.clock);
	INFO("Seed: %i", game_state.seed);
	INFO("Tick: %zu", game_state.time);
	INFO("");
	INFO("[ACTORS]");
	INFO("	Latest Actor: %i (%p)", game_state.live_actors, get_actor(game_state.live_actors));
	INFO("	Next Actor ID: %i", game_state.next_actor);
	INFO("");

	for (const GameActor* actor = get_actor(game_state.live_actors); actor != NULL;
		actor = get_actor(actor->previous))
	{
		INFO("	Actor %i:", actor->id);
		INFO("		Type: %i", actor->type);
		INFO("		Previous Actor: %i (%p)", actor->previous, get_actor(actor->previous));
		INFO("		Next Actor: %i (%p)", actor->next, get_actor(actor->next));
		INFO("		Previous Neighbor: %i (%p)", actor->previous_cell, get_actor(actor->previous_cell));
		INFO("		Next Neighbor: %i (%p)", actor->next_cell, get_actor(actor->next_cell));
		INFO("		Cell: %i", actor->cell);
		INFO("		Position: (%.2f, %.2f)", FtFloat(actor->pos.x), FtFloat(actor->pos.y));
		INFO("		Box: (%.2f, %.2f, %.2f, %.2f)", FtFloat(actor->box.start.x),
			FtFloat(actor->box.start.y), FtFloat(actor->box.end.x), FtFloat(actor->box.end.y));
		INFO("		Depth: %.2f", FtFloat(actor->depth));
		INFO("		Flags: %u", actor->flags);
		INFO("		Non-Zero Values:");

		for (ActorValue i = 0; i < MAX_VALUES; i++)
			if (actor->values[i] != 0L)
				INFO("			%u: %i", i, actor->values[i]);

		INFO("");
	}

	INFO("");
	INFO("	(Grid...)");
	INFO("");
	INFO("[STRINGS]");
	INFO("	Level: %s", game_state.level);
	INFO("	Next Level: %s", game_state.next);
	INFO("====================");
}

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
	load_font("main");

	load_sound("hurry");
	load_sound("tick");
	load_sound("pause");
	load_sound("select");
	load_sound("chat");

	load_track("yi_score");

	//
	//
	//
	// LEVEL LOADER
	//
	//
	//
	const char* kla = find_data_file(fmt("data/levels/%s.*", ctx->level), NULL);
	if (kla == NULL) {
		show_error("Level \"%s\" not found", ctx->level);
		goto level_fail;
	}
	uint8_t* data = SDL_LoadFile(kla, NULL);
	if (data == NULL) {
		show_error("Failed to load level \"%s\"\n%s", ctx->level, SDL_GetError());
		goto level_fail;
	}
	const uint8_t* buf = data;

	// Header
	if (SDL_strncmp((const char*)buf, "Klawiatura", 10) != 0) {
		SDL_free(data);
		show_error("Invalid header in level \"%s\"", ctx->level);
		goto level_fail;
	}
	buf += 10;

	const uint8_t major = *buf;
	if (major != MAJOR_LEVEL_VERSION) {
		SDL_free(data);
		show_error("Invalid major version in level \"%s\"\nLevel: %u\nGame: %u)", ctx->level, major,
			MAJOR_LEVEL_VERSION);
		goto level_fail;
	}
	buf++;

	const uint8_t minor = *buf;
	if (minor != MINOR_LEVEL_VERSION) {
		SDL_free(data);
		show_error("Invalid minor version in level \"%s\"\nLevel: %u\nGame: %u)", ctx->level, minor,
			MINOR_LEVEL_VERSION);
		goto level_fail;
	}
	buf++;

	// Level
	char level_name[32];
	SDL_memcpy(level_name, buf, 32);
	INFO("Level: %s (%s)", ctx->level, level_name);
	buf += 32;

	read_string((const char**)(&buf), video_state.world, sizeof(video_state.world));
	if (video_state.world[0] != '@')
		load_texture(video_state.world);

	read_string((const char**)(&buf), game_state.next, sizeof(game_state.next));

	char track_name[2][GAME_STRING_MAX];
	for (int i = 0; i < 2; i++) {
		read_string((const char**)(&buf), track_name[i], sizeof(track_name));
		load_track(track_name[i]);
	}

	game_state.flags |= *((GameFlag*)buf), buf += sizeof(GameFlag);
	if (game_state.flags & GF_AMBUSH)
		game_state.sequence.type = SEQ_AMBUSH;

	game_state.size.x = *((fixed*)buf), buf += sizeof(fixed);
	game_state.size.y = *((fixed*)buf), buf += sizeof(fixed);

	game_state.bounds.start.x = *((fixed*)buf), buf += sizeof(fixed);
	game_state.bounds.start.y = *((fixed*)buf), buf += sizeof(fixed);
	game_state.bounds.end.x = *((fixed*)buf), buf += sizeof(fixed);
	game_state.bounds.end.y = *((fixed*)buf), buf += sizeof(fixed);

	game_state.clock = *((int32_t*)buf), buf += sizeof(int32_t);
	game_state.water = *((fixed*)buf), buf += sizeof(fixed);
	game_state.hazard = *((fixed*)buf), buf += sizeof(fixed);

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
		default: {
			SDL_free(data);
			show_error("Unknown marker type %u", marker_type);
			goto level_fail;
		}

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

				buf += sizeof(fixed), buf += sizeof(fvec2);

				uint8_t num_values = *((uint8_t*)buf);
				buf++;
				for (uint8_t j = 0; j < num_values; j++)
					buf += 1 + sizeof(ActorValue);
				buf += sizeof(ActorFlag);

				break;
			}

			actor->depth = *((fixed*)buf), buf += sizeof(fixed);

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
				actor->values[idx] = *((ActorValue*)buf), buf += sizeof(ActorValue);
			}
			FLAG_ON(actor, *((ActorFlag*)buf)), buf += sizeof(ActorFlag);

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
	return;

level_fail:
	nuke_game();
	disconnect();
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

	if (player->lives < 0L && !(game_state.flags & GF_KEVIN)) {
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

	GameActor* kevin = get_actor(player->kevin.actor);
	if (kevin != NULL)
		FLAG_ON(kevin, FLG_DESTROY);
	player->kevin.actor = NULLACT;

	GameActor* spawn = get_actor(game_state.autoscroll);
	if (spawn == NULL)
		spawn = get_actor(game_state.checkpoint);
	if (spawn == NULL)
		spawn = get_actor(game_state.spawn);
	if (spawn == NULL)
		return NULL;

	if (spawn->type == ACT_CHECKPOINT && VAL(spawn, CHECKPOINT_BOUNDS_X1) != VAL(spawn, CHECKPOINT_BOUNDS_X2)
		&& VAL(spawn, CHECKPOINT_BOUNDS_Y1) != VAL(spawn, CHECKPOINT_BOUNDS_Y2))
	{
		player->bounds.start.x = VAL(spawn, CHECKPOINT_BOUNDS_X1),
		player->bounds.start.y = VAL(spawn, CHECKPOINT_BOUNDS_Y1),
		player->bounds.end.x = VAL(spawn, CHECKPOINT_BOUNDS_X2),
		player->bounds.end.y = VAL(spawn, CHECKPOINT_BOUNDS_Y2);
	} else
		SDL_memcpy(&player->bounds, &game_state.bounds, sizeof(game_state.bounds));

	pawn = create_actor(
		ACT_PLAYER, (spawn->type == ACT_AUTOSCROLL)
				    ? (fvec2){Flerp(game_state.bounds.start.x, game_state.bounds.end.x, FxHalf),
					      game_state.bounds.start.y}
				    : spawn->pos);
	if (pawn != NULL) {
		VAL(pawn, PLAYER_INDEX) = (ActorValue)player->id;
		player->actor = pawn->id;
		player->pos = player->kevin.start = pawn->pos;
		player->kevin.delay = 0L;

		FLAG_ON(pawn, spawn->flags & (FLG_X_FLIP | FLG_PLAYER_ASCEND | FLG_PLAYER_DESCEND));
		if (ANY_FLAG(pawn, FLG_PLAYER_ASCEND))
			VAL(pawn, Y_SPEED) -= FfInt(22L);

		if (spawn->type == ACT_AUTOSCROLL)
			FLAG_ON(pawn, FLG_PLAYER_RESPAWN);

		// !!! CLIENT-SIDE !!!
		if (local_player == player->id)
			set_view_player(player);
		// !!! CLIENT-SIDE !!!
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
	if (player == NULL) {
		view_player = NULLPLAY;
		return;
	}
	if (view_player == player->id)
		return;
	GamePlayer* oplayer = get_player(view_player);
	if (oplayer != NULL && Rcollide(oplayer->bounds, player->bounds))
		lerp_camera(25.f);

	view_player = player->id;
}

// ======
// ACTORS
// ======

/// Load an actor.
void load_actor(GameActorType type) {
	if (type <= ACT_NULL || type >= ACT_SIZE)
		INFO("Loading invalid actor %u", type);
	else
		ACTOR_CALL_STATIC(type, load);
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
	iactor->from = iactor->pos = pos;

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
Bool below_level(const GameActor* actor) {
	return actor != NULL && (actor->pos.y + actor->box.start.y) > game_state.size.y;
}

/// Check if the actor is within any player's range.
#define CHECK_AUTOSCROLL_VIEW                                                                                          \
	const GameActor* autoscroll = get_actor(game_state.autoscroll);                                                \
	if (autoscroll != NULL) {                                                                                      \
		const frect cbox = {Vsub(game_state.bounds.start, (fvec2){padding, padding}),                          \
			Vadd(game_state.bounds.end, (fvec2){padding, padding})};                                       \
		return (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y            \
			&& (ignore_top || abox.end.y > cbox.start.y));                                                 \
	}

Bool in_any_view(const GameActor* actor, fixed padding, Bool ignore_top) {
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
Bool in_player_view(const GameActor* actor, GamePlayer* player, fixed padding, Bool ignore_top) {
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
				if (ACTOR_IS_SOLID(actor, types) && actor->type != ACT_SOLID_SLOPE
					&& Rcollide(rect, HITBOX(actor)))
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
		VAL(actor, X_SPEED) = VAL(actor, Y_SPEED) = FxZero;
		VAL(actor, X_TOUCH) = ((shift >= FxZero) ? -1L : 1L), VAL(actor, Y_TOUCH) = 1L;
		return;
	}

	VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0L;
	fixed x = actor->pos.x, y = actor->pos.y;

	// Horizontal collision
	x += VAL(actor, X_SPEED);
	CellList list = {0};
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });
	Bool climbed = false;

	if (list.num_actors > 0L) {
		Bool stop = false;
		if (VAL(actor, X_SPEED) < FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID)
					|| displacer->type == ACT_SOLID_SLOPE)
					continue;

				if (VAL(actor, Y_SPEED) >= FxZero
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
						VAL(actor, Y_SPEED) = FxZero;
						VAL(actor, Y_TOUCH) = 1L;
						climbed = true;
					}
					continue;
				}

				ACTOR_CALL2(displacer, on_right, actor);
				x = Fmax(x, displacer->pos.x + displacer->box.end.x - actor->box.start.x);
				stop = VAL(actor, X_SPEED) <= FxZero;
				climbed = false;
			}
			VAL(actor, X_TOUCH) = -(stop && !climbed);
		} else if (VAL(actor, X_SPEED) > FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID)
					|| displacer->type == ACT_SOLID_SLOPE)
					continue;

				if (VAL(actor, Y_SPEED) >= FxZero
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
						VAL(actor, Y_SPEED) = FxZero;
						VAL(actor, Y_TOUCH) = 1L;
						climbed = true;
					}
					continue;
				}

				ACTOR_CALL2(displacer, on_left, actor);
				x = Fmin(x, displacer->pos.x + displacer->box.start.x - actor->box.end.x);
				stop = VAL(actor, X_SPEED) >= FxZero;
				climbed = false;
			}
			VAL(actor, X_TOUCH) = stop && !climbed;
		}

		if (stop)
			VAL(actor, X_SPEED) = FxZero;
	}

	// Vertical collision
	y += VAL(actor, Y_SPEED);
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		Bool stop = false;
		if (VAL(actor, Y_SPEED) < FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID)
					|| (displacer->type == ACT_SOLID_SLOPE
						&& (ANY_FLAG(displacer, FLG_Y_FLIP)
							|| actor->pos.y < (displacer->pos.y + displacer->box.end.y))))
					continue;

				ACTOR_CALL2(displacer, on_bottom, actor);
				y = Fmax(y, displacer->pos.y + displacer->box.end.y - actor->box.start.y);
				stop = VAL(actor, Y_SPEED) <= FxZero;
			}
			VAL(actor, Y_TOUCH) = -(fixed)stop;
		} else if (VAL(actor, Y_SPEED) > FxZero) {
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer)
					continue;

				if (displacer->type == ACT_SOLID_SLOPE) {
					const Bool side = ANY_FLAG(displacer, FLG_X_FLIP);

					const fixed sa = displacer->pos.y
					                 + (side ? displacer->box.start.y : displacer->box.end.y),
						    sb = displacer->pos.y
					                 + (side ? displacer->box.end.y : displacer->box.start.y);

					const fixed ax = x + (side ? actor->box.start.x : actor->box.end.x),
						    sx = displacer->pos.x
					                 + (side ? displacer->box.end.x : displacer->box.start.x);

					const fixed slope = Flerp(sa, sb,
						Fclamp(Fdiv(ax - sx, displacer->box.end.x - displacer->box.start.x),
							FxZero, FxOne));

					if (y >= slope) {
						y = slope;
						stop = true;
					}
					continue;
				}

				const SolidType solid = ACTOR_GET_SOLID(displacer);
				if (!(solid & SOL_ALL)
					|| ((solid & SOL_TOP)
						&& (y + actor->box.end.y - VAL(actor, Y_SPEED))
							   > (displacer->pos.y + displacer->box.start.y + climb)))
					continue;

				ACTOR_CALL2(displacer, on_top, actor);
				y = Fmin(y, displacer->pos.y + displacer->box.start.y - actor->box.end.y);
				stop = VAL(actor, Y_SPEED) >= FxZero;
			}
			VAL(actor, Y_TOUCH) = (fixed)stop;
		}

		if (stop)
			VAL(actor, Y_SPEED) = FxZero;
	}

	move_actor(actor, (fvec2){x, y});
}

/// Move the actor with speed and touch solid actors without being pushed away.
// FIXME: Implement slope collisions for this variant.
void displace_actor_soft(GameActor* actor) {
	if (actor == NULL)
		return;

	VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0L;
	fixed x = actor->pos.x, y = actor->pos.y;

	// Horizontal collision
	x += VAL(actor, X_SPEED);
	CellList list = {0};
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		if (VAL(actor, X_SPEED) < FxZero)
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_right, actor);
				VAL(actor, X_TOUCH) = -1L;
			}
		else if (VAL(actor, X_SPEED) > FxZero)
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_left, actor);
				VAL(actor, X_TOUCH) = 1L;
			}
	}

	// Vertical collision
	y += VAL(actor, Y_SPEED);
	list_cell_at(&list, (frect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		if (VAL(actor, Y_SPEED) < FxZero)
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_bottom, actor);
				VAL(actor, Y_TOUCH) = -1L;
			}
		else if (VAL(actor, Y_SPEED) > FxZero)
			for (ActorID i = 0; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer)
					continue;

				const SolidType solid = ACTOR_GET_SOLID(displacer);
				if (!(solid & SOL_ALL)
					|| ((solid & SOL_TOP)
						&& (y + actor->box.end.y - VAL(actor, Y_SPEED))
							   > (displacer->pos.y + displacer->box.start.y)))
					continue;

				ACTOR_CALL2(displacer, on_top, actor);
				VAL(actor, Y_TOUCH) = 1L;
			}
	}

	move_actor(actor, (fvec2){x, y});
}

/// Generic interpolated actor drawing function.
//
// Formula for current static actor frame: `(game_state.time / ((TICKRATE * 2) / speed)) % frames`
void draw_actor(const GameActor* actor, const char* name, GLfloat angle, const GLubyte color[4]) {
	const InterpActor* iactor = get_interp(actor);
	const ActorValue sprout = VAL(actor, SPROUT);
	const GLfloat z = (sprout > 0L) ? 21.f : FtFloat(actor->depth);
	batch_start(XYZ((int)(FtFloat(iactor->pos.x) - camera_offset_morsel[0]),
			    (int)(FtFloat(iactor->pos.y) + FtFloat(sprout) + camera_offset_morsel[1]), z),
		angle, color);
	batch_sprite(name, FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
}

void draw_dead(const GameActor* actor) {
	const GameActorType type = VAL(actor, DEAD_TYPE);
	if (ACTORS[type] != NULL && ACTORS[type]->draw_dead != NULL)
		ACTORS[type]->draw_dead(actor);
}

/// Convenience function for quaking at an actor's position.
void quake_at_actor(const GameActor* actor, float amount) {
	const float dist = glm_vec2_distance(video_state.camera.pos, (vec2){FtInt(actor->pos.x), FtInt(actor->pos.y)})
	                   / (float)SCREEN_HEIGHT;
	const float quake = amount / SDL_max(dist, 1.f);
	video_state.quake = SDL_min(video_state.quake + quake, 10.f);
}

/// Convenience function for playing a sound at an actor's position.
void play_actor_sound(const GameActor* actor, const char* name) {
	play_state_sound_at(name, (float[2]){FtFloat(actor->pos.x), FtFloat(actor->pos.y)});
}

// ====
// MATH
// ====

/// Produce an exclusive random number.
// https://rosettacode.org/wiki/Linear_congruential_generator
int32_t rng(int32_t x) {
	if (x <= 0L)
		return 0;
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

/// Start interpolating an actor's position from another actor. Used for trails.
void align_interp(const GameActor* actor, const GameActor* from) {
	if (BAD_ACTOR(actor) || BAD_ACTOR(from))
		return;
	InterpActor *iactor = &interp.actors[actor->id], *ifrom = &interp.actors[from->id];
	iactor->from = ifrom->from, iactor->pos = ifrom->pos;
}

#undef BAD_ACTOR
