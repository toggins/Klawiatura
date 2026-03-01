#include "K_assets.h"
#include "K_cmd.h"
#include "K_discord.h"
#include "K_editor.h"
#include "K_file.h"
#include "K_game.h"
#include "K_input.h"
#include "K_log.h"
#include "K_menu.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"

#include "actors/K_autoscroll.h"
#include "actors/K_block.h"
#include "actors/K_checkpoint.h" // IWYU pragma: keep
#include "actors/K_enemies.h"    // IWYU pragma: keep
#include "actors/K_player.h"
#include "actors/K_powerups.h"
#include "actors/K_warp.h"

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
GekkoSession* game_session = NULL;

GameContext queue_game_context = {0}, game_context = {0};
GameState game_state = {0};

PlayerID local_player = NULLPLAY, view_player = NULLPLAY;

static InterpState interp = {0};

// =====
// PAUSE
// =====

static Bool paused = FALSE;

void pause_gamestate() {
	paused = TRUE;
	if (!NutPunch_IsReady())
		pause_audio_state(TRUE);
	input_wipeout();
}

void unpause_gamestate() {
	paused = FALSE;
	pause_audio_state(FALSE);
	input_wipeout();
}

// ====
// GAME
// ====

static Bool rolling_back = FALSE;

/// Initializes a GameContext struct with a clean 1-player preset.
GameContext* init_game_context() {
	SDL_zero(queue_game_context);
	queue_game_context.num_players = 1L;
	for (PlayerID i = 0L; i < MAX_PLAYERS; i++) {
		queue_game_context.players[i].power = POW_SMALL;
		queue_game_context.players[i].lives = DEFAULT_LIVES;
	}
	queue_game_context.checkpoint = NULLACT;
	return &queue_game_context;
}

static void start_game_state();
void start_game() {
	set_editing_level(FALSE);

	if (game_exists())
		nuke_game();
	else
		clear_assets();

	EXPECT(queue_game_context.num_players > 0L && queue_game_context.num_players <= MAX_PLAYERS,
		"Invalid player count in game context!\nExpected 1..%li, got %i", MAX_PLAYERS,
		queue_game_context.num_players);
	game_context = queue_game_context;

	load_texture("ui/sidebar_l");
	load_texture("ui/sidebar_r");

	game_surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);

	const Bool spectating = i_am_spectating();
	gekko_create(&game_session, spectating ? GekkoSpectateSession : GekkoGameSession);

	GekkoConfig cfg = {0};
	cfg.desync_detection = TRUE;
	cfg.input_size = sizeof(GameInput);
	cfg.state_size = sizeof(SaveState);
	cfg.input_prediction_window = MAX_INPUT_DELAY;
	cfg.num_players = game_context.num_players;
	if (!spectating)
		for (int i = 0; i < MAX_PEERS; i++)
			if (NutPunch_PeerAlive(i) && peer_is_spectating(i))
				++cfg.max_spectators;

	unpause_gamestate();
	gekko_start(game_session, &cfg);
	local_player = view_player = populate_game(game_session);
	populate_results();

	start_audio_state();
	start_video_state();
	start_game_state();
	from_scratch();
	input_wipeout();

	extern void enter_gaming_menu();
	enter_gaming_menu();
}

void continue_game() {
	if (!NutPunch_IsReady()) {
		start_game();
		return;
	}

	if (!NutPunch_IsMaster())
		return;

	char *data = net_buffer(), *cursor = data;
	*(PacketType*)cursor = PT_CONTINUE, cursor += sizeof(PacketType);
	SDL_strlcpy(cursor, queue_game_context.level, CLIENT_STRING_MAX), cursor += CLIENT_STRING_MAX;
	*(GameFlag*)cursor = queue_game_context.flags, cursor += sizeof(GameFlag);
	*(ActorID*)cursor = queue_game_context.checkpoint, cursor += sizeof(ActorID);

	PlayerID count = 0;
	for (int i = 0; i < MAX_PEERS; i++)
		if (NutPunch_PeerAlive(i) && !peer_is_spectating(i))
			++count;

	*(PlayerID*)cursor = queue_game_context.num_players = count, cursor += sizeof(PlayerID);
	for (PlayerID i = 0; i < count; i++) {
		*(Sint8*)cursor = queue_game_context.players[i].lives, cursor += sizeof(Sint8);
		*(PlayerPower*)cursor = queue_game_context.players[i].power, cursor += sizeof(PlayerPower);
		*(Uint8*)cursor = queue_game_context.players[i].coins, cursor += sizeof(Uint8);
		*(Uint32*)cursor = queue_game_context.players[i].score, cursor += sizeof(Uint32);
	}

	for (int i = 0; i < MAX_PEERS; i++)
		NutPunch_SendReliably(PCH_LOBBY, i, data, (int)(cursor - data));

	start_game();
}

Bool game_exists() {
	return game_session != NULL;
}

static void nuke_game_state();
void nuke_game() {
	set_editing_level(FALSE);

	if (game_session == NULL)
		return;

	nuke_game_state();
	nuke_audio_state();
	nuke_video_state();

	gekko_destroy(&game_session);
	game_session = NULL;
	destroy_surface(game_surface);
	game_surface = NULL;

	update_discord_status(NULL);
}

static void nuke_game_to_menu() {
	nuke_game();
	clear_assets(), load_menu();
}

static Uint32 check_game_state();
static void dump_game_state(), save_game_state(GameState*), load_game_state(const GameState*),
	tick_game_state(const GameInput[MAX_PLAYERS]);

static void update_interp_parameters() {
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
}

void interpolate() {
	if (!game_exists())
		return;

	register const Fixed ftick = FxFrom(pendingticks());
	for (const GameActor* actor = get_actor(game_state.live_actors); actor; actor = get_actor(actor->previous)) {
		InterpActor* iactor = &interp.actors[actor->id];
		iactor->pos = Vadd(iactor->from, Vscale(Vsub(actor->pos, iactor->from), ftick));
	}
}

static void add_local_inputs() {
	GameInput input = 0;
	if (!paused && !typing_what()) {
		input |= kb_down(KB_UP) * GI_UP;
		input |= kb_down(KB_LEFT) * GI_LEFT;
		input |= kb_down(KB_DOWN) * GI_DOWN;
		input |= kb_down(KB_RIGHT) * GI_RIGHT;
		input |= kb_down(KB_JUMP) * GI_JUMP;
		input |= kb_down(KB_FIRE) * GI_FIRE;
		input |= kb_down(KB_RUN) * GI_RUN;
	}
	gekko_add_local_input(game_session, local_player, &input);
}

static Bool handle_session_events() {
	int count = 0;
	GekkoSessionEvent** events = gekko_session_events(game_session, &count);
	for (int i = 0; i < count; i++) {
		GekkoSessionEvent* event = events[i];
		switch (event->type) {
		case GekkoDesyncDetected: {
			dump_game_state();
			struct GekkoDesynced desync = event->data.desynced;
			show_error("Out of sync with player %i (%s)\n"
				   "Tick: %i\n"
				   "Local Checksum: %u\n"
				   "Remote Checksum: %u\n"
				   "\nGame state has been dumped to console.",
				desync.remote_handle + 1, who_is_winner(desync.remote_handle), desync.frame,
				desync.local_checksum, desync.remote_checksum);
			return FALSE;
		}

		case GekkoPlayerConnected: {
			struct GekkoConnected connect = event->data.connected;
			INFO("Player %i connected", connect.handle + 1);
			break;
		}

		case GekkoPlayerDisconnected: {
			struct GekkoDisconnected disconnect = event->data.disconnected;

			if (spectator_to_peer(disconnect.handle) < MAX_PEERS) {
				WARN("Spectator %i left the session", disconnect.handle + 1);
				break;
			}

			show_error("Lost connection to player %i (%s)", disconnect.handle + 1,
				who_is_winner(disconnect.handle));
			return FALSE;
		}

		default:
			break;
		}
	}

	return TRUE;
}

void restart_game_session() {
	GameContext* ctx = init_game_context();
	SDL_strlcpy(ctx->level, game_context.level, sizeof(ctx->level));
	ctx->flags |= GF_REPLAY | GF_TRY_HELL;
	ctx->num_players = game_context.num_players;
	for (PlayerID i = 0L; i < ctx->num_players; i++) {
		ctx->players[i].lives = game_state.players[i].lives;
		ctx->players[i].coins = game_state.players[i].coins;
		ctx->players[i].score = game_state.players[i].score;
	}
	ctx->checkpoint = game_state.checkpoint;
	continue_game();
}

static Bool handle_advance(const GekkoGameEvent* event) {
	rolling_back = event->data.adv.rolling_back;

	GameInput inputs[MAX_PLAYERS] = {0L};
	for (PlayerID j = 0L; j < game_context.num_players; j++)
		inputs[j] = ((GameInput*)event->data.adv.inputs)[j];
	tick_game_state(inputs);
	tick_audio_state();

	if (game_state.flags & GF_RESTART) {
		restart_game_session();
		return TRUE;
	}

	if (!(game_state.flags & GF_END))
		return TRUE;

	if (!game_state.next[0]) {
		show_results();
		return FALSE;
	}

	GameContext* ctx = init_game_context();
	extern GameState game_state;

	SDL_strlcpy(ctx->level, (char*)game_state.next, sizeof(ctx->level));
	ctx->flags |= GF_TRY_HELL;
	ctx->num_players = numplayers();
	for (PlayerID i = 0L; i < ctx->num_players; i++) {
		const Sint8 lives = game_state.players[i].lives;
		if (lives < 0L && !(ctx->flags & GF_HELL))
			continue;
		ctx->players[i].lives = lives;
		ctx->players[i].power = game_state.players[i].power;
		ctx->players[i].coins = game_state.players[i].coins;
		ctx->players[i].score = game_state.players[i].score;
	}

	continue_game();
	return TRUE;
}

static Bool handle_session_update() {
	int count = 0;
	GekkoGameEvent** updates = gekko_update_session(game_session, &count);
	for (int i = 0; i < count; i++) {
		GekkoGameEvent* event = updates[i];
		switch (event->type) {
		case GekkoSaveEvent:
			static SaveState save;
			save_game_state(&save.game);
			save_audio_state(&save.audio);

			*event->data.save.state_len = sizeof(save);
			*event->data.save.checksum = check_game_state();
			SDL_memcpy(event->data.save.state, &save, sizeof(save));
			break;

		case GekkoLoadEvent:
			const SaveState* load = (SaveState*)(event->data.load.state);
			load_game_state(&load->game);
			load_audio_state(&load->audio);
			break;

		case GekkoAdvanceEvent:
			if (!handle_advance(event))
				return FALSE;
			break;

		default:
			break;
		}
	}

	return TRUE;
}

Bool update_game() {
	// Safety net, NutPunch errors can nuke the game at any moment.
	if (game_session == NULL)
		goto byebye_game;

	update_interp_parameters();

	if (paused && !NutPunch_IsReady())
		return TRUE;

	if (!NutPunch_IsReady() && kb_pressed(KB_EDIT))
		set_editing_level(!is_editing_level());

	if (!i_am_spectating())
		add_local_inputs();

	if (!handle_session_events())
		goto byebye_game;
	if (!handle_session_update())
		goto byebye_game;

	return TRUE;

byebye_game:
	nuke_game_to_menu();
	return FALSE;
}

static mat4 proj = GLM_MAT4_IDENTITY;
static vec2 camera_offset_morsel = {0.f, 0.f};

static void perform_camera_magic() {
	static vec2 cpos = GLM_VEC2_ZERO;
	VideoCamera* camera = &video_state.camera;
	camera_offset_morsel[0] = camera_offset_morsel[1] = 0.f;

	const GamePlayer* player = get_player(view_player);
	const GameActor* autoscroll = get_actor(game_state.autoscroll);

#define MORSEL()                                                                                                       \
	do {                                                                                                           \
		camera->pos[0] = Fx2Int(xx), camera->pos[1] = Fx2Int(yy);                                              \
		camera->pos[0] = SDL_clamp(camera->pos[0], bx1, bx2);                                                  \
		camera->pos[1] = SDL_clamp(camera->pos[1], by1, by2);                                                  \
		camera_offset_morsel[0] = SDL_clamp(Fx2Float(xx), bx1, bx2) - camera->pos[0];                          \
		camera_offset_morsel[1] = camera->pos[1] - SDL_clamp(Fx2Float(yy), by1, by2);                          \
	} while (0)

	if (autoscroll) {
		const InterpActor* iautoscroll = get_interp(autoscroll);
		if (!iautoscroll)
			goto skip_morsel;

		const Fixed xx = iautoscroll->pos.x + F_HALF_SCREEN_WIDTH,
			    yy = iautoscroll->pos.y + F_HALF_SCREEN_HEIGHT;
		const float bx1 = HALF_SCREEN_WIDTH, by1 = HALF_SCREEN_HEIGHT,
			    bx2 = Fx2Int(game_state.size.x - F_HALF_SCREEN_WIDTH),
			    by2 = Fx2Int(game_state.size.y - F_HALF_SCREEN_HEIGHT);
		MORSEL();
	} else if (player) {
		Fixed xx = FxZero, yy = FxZero;

		const InterpActor* ipawn = get_interp(get_actor(player->actor));
		if (ipawn)
			xx = ipawn->pos.x, yy = ipawn->pos.y;
		else
			xx = player->pos.x, yy = player->pos.y;

		const float bx1 = Fx2Int(player->bounds.start.x + F_HALF_SCREEN_WIDTH),
			    by1 = Fx2Int(player->bounds.start.y + F_HALF_SCREEN_HEIGHT),
			    bx2 = Fx2Int(player->bounds.end.x - F_HALF_SCREEN_WIDTH),
			    by2 = Fx2Int(player->bounds.end.y - F_HALF_SCREEN_HEIGHT);
		MORSEL();
	}
#undef MORSEL

skip_morsel:
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

	for (const GameActor* actor = get_actor(game_state.live_actors); actor != NULL;
		actor = get_actor(actor->previous))
		ACTOR_CALL(actor, draw_hud);

	if (video_state.message[0] != '\0') {
		const float ease = 1.f - (SDL_min(video_state.message_time, 30.f) / 30.f);
		const float my = glm_lerp(-24.f, 112.f, 1.f - (ease * ease));

		batch_reset();
		batch_pos(B_XYZ(HALF_SCREEN_WIDTH, my, -10000.f)), batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_color(B_ALPHA((1.f - (SDL_max(video_state.message_time - 200.f, 0.f) / 50.f)) * 255L));
		batch_string("main", 24.f, video_state.message);

		video_state.message_time += dt();
		if (video_state.message_time >= 250.f) {
			video_state.message[0] = '\0';
			video_state.message_time = 0.f;
		}
	}

	batch_pos(B_XYZ(32.f, 16.f, -10000.f)), batch_color(B_WHITE), batch_align(B_TOP_LEFT);
	batch_string("hud", 16, fmt("MARIO * %u", SDL_max(player->lives, 0)));
	batch_pos(B_XYZ(147.f, 34.f, -10000.f));
	batch_align(B_TOP_RIGHT);
	batch_string("hud", 16.f, fmt("%u", player->score));

	const char* tex = NULL;
	switch ((int)((float)(game_state.time) / 6.25f) % 4L) {
	default:
		tex = "ui/coins0";
		break;
	case 1L:
	case 3L:
		tex = "ui/coins1";
		break;
	case 2L:
		tex = "ui/coins2";
		break;
	}
	batch_pos(B_XYZ(224.f, 34.f, -10000.f));
	batch_sprite(tex);
	batch_pos(B_XYZ(288.f, 34.f, -10000.f));
	batch_string("hud", 16.f, fmt("%u", player->coins));

	batch_pos(B_XYZ(432.f, 16.f, -10000.f));
	if (video_state.world[0] == ')') {
		batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_string("hud", 16.f, "WORLD");
		batch_pos(B_XYZ(432.f, 34.f, -10000.f));
		batch_string("hud", 16.f, video_state.world + 1L);
	} else if (video_state.world[0] == ']') {
		batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_string("hud", 16.f, video_state.world + 1L);
	} else
		batch_sprite(video_state.world);

	if (game_state.clock >= 0L) {
		float scale = 1.f;
		if ((game_state.flags & GF_HURRY) && video_state.hurry < 120.f) {
			video_state.hurry += dt();
			if (video_state.hurry < 8.f)
				scale = 1.f + ((video_state.hurry / 8.f) * 0.6f);
			else if (video_state.hurry >= 8.f && video_state.hurry <= 112.f)
				scale = 1.6f;
			else if (video_state.hurry > 112.f)
				scale = 1.6f - (((video_state.hurry - 112.f) / 8.f) * 0.6f);
		}

		const float x = (float)SCREEN_WIDTH - (32.f * scale);
		const float size = 16.f * scale;

		batch_pos(B_XYZ(x, 24.f * scale, -10000.f));
		const float yscale = (video_state.hurry > 0.f && video_state.hurry < 120.f)
		                             ? glm_lerp(1.f, 0.8f + (SDL_sinf(video_state.hurry * 0.6f) * 0.2f),
						       (scale - 1.0f) / 0.6f)
		                             : 1.f;
		batch_scale(B_XY(1.f, yscale)), batch_align(B_ALIGN(FA_RIGHT, FA_MIDDLE));
		batch_string("hud", size, "TIME");
		batch_scale(B_SCALE(1.f));

		batch_pos(B_XYZ(x, 34.f * scale, -10000.f)), batch_align(B_TOP_RIGHT);
		batch_string("hud", size, fmt("%u", game_state.clock));
	}

	if (game_state.sequence.type == SEQ_LOSE && game_state.sequence.time > 0L) {
		batch_pos(B_XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, -10000.f)), batch_align(B_CENTER);
		batch_string("hud", 16, (game_state.clock == 0) ? "TIME UP" : "GAME OVER");
	} else if (local_player != view_player) {
		batch_pos(B_XYZ(32, 64, -10000.f)), batch_color(B_ALPHA(160)), batch_align(B_TOP_LEFT);
		batch_string("main", 24, fmt("Spectating: %s", get_peer_name(player_to_peer(view_player))));
	}
}

void draw_game() {
	if (!game_exists())
		return;

	batch_reset();
	batch_sprite("ui/sidebar_l"), batch_sprite("ui/sidebar_r");

	push_surface(game_surface);

	perform_camera_magic();
	clear_depth(1.f);

	draw_tilemaps();

	const GameActor* wave = get_actor(game_state.wave);
	if (wave != NULL && ANY_FLAG(wave, FLG_WAVE_SKY)) {
		batch_pos(B_XYZ(0.f, video_state.camera.pos[1] - HALF_SCREEN_HEIGHT, 200.f));
		batch_colors((Uint8[4][4]){
			{59,  123, 163, 255},
                        {59,  123, 163, 255},
                        {242, 253, 252, 255},
                        {242, 253, 252, 255}
                });
		batch_rectangle(NULL, B_XY(Fx2Float(game_state.size.x), SCREEN_HEIGHT));
	}

	const GameActor* actor = get_actor(game_state.live_actors);
	while (actor != NULL) {
		if (ANY_FLAG(actor, FLG_VISIBLE))
			ACTOR_CALL(actor, draw);
		actor = get_actor(actor->previous);
	}

	float wy = Fx2Int(game_state.water);
	const float lh = Fx2Float(game_state.size.y);
	if (wy < lh) {
		const float lw = Fx2Float(game_state.size.x);
		batch_reset(), batch_pos(B_XYZ(0.f, wy, -100.f)), batch_color(B_ALPHA(135));
		batch_tile(B_TILE(TRUE, FALSE));
		batch_rectangle(fmt("markers/water%u", (game_state.time / 5L) % 8L), B_XY(lw, 16.f));
		wy += 16.f;
		if (wy < lh) {
			batch_pos(B_XYZ(0.f, wy, -100.f)), batch_color(B_RGBA(88, 136, 224, 135));
			batch_rectangle(NULL, B_XY(lw, lh - wy));
		}
	}

	pop_surface();

	batch_reset();
	if (game_state.sequence.type == SEQ_SECRET) {
		set_shader(SH_WAVE);
		set_float_uniform(UNI_TIME, totalticks() * 0.05f);
		set_vec2_uniform(UNI_WAVE, B_XY(game_state.sequence.time * 0.00075f, 20.f));
		const float t = SDL_min((float)game_state.sequence.time, 100.f) / 100.f;
		batch_color(B_RGB(glm_lerp(255.f, 50.f, t), glm_lerp(255.f, 80.f, t), glm_lerp(255.f, 200.f, t)));
		batch_surface(game_surface);
		set_shader(SH_MAIN);
	} else
		batch_surface(game_surface);

	draw_hud();
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
static void tick_game_state(const GameInput inputs[MAX_PLAYERS]) {
	for (PlayerID i = 0L; i < game_context.num_players; i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;
		player->last_input = player->input;
		player->input = inputs[i];
	}

	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll != NULL) {
		game_state.bounds.start.x = Fclamp(autoscroll->pos.x, FxZero, game_state.size.x - F_SCREEN_WIDTH);
		game_state.bounds.end.x = Fclamp(autoscroll->pos.x + F_SCREEN_WIDTH, F_SCREEN_WIDTH, game_state.size.x);
		game_state.bounds.start.y = Fclamp(autoscroll->pos.y, FxZero, game_state.size.y - F_SCREEN_HEIGHT);
		game_state.bounds.end.y
			= Fclamp(autoscroll->pos.y + F_SCREEN_HEIGHT, F_SCREEN_HEIGHT, game_state.size.y);
	}

	GameActor* wave = get_actor(game_state.wave);
	if (wave != NULL) {
		const Fixed delta = VAL(wave, WAVE_DELTA);

		game_state.bounds.start.y -= delta, game_state.bounds.end.y -= delta;
		for (PlayerID i = 0L; i < game_context.num_players; i++) {
			GamePlayer* player = get_player(i);
			if (player == NULL)
				continue;
			player->bounds.start.y -= delta, player->bounds.end.y -= delta;
		}

		game_state.water -= VAL(wave, WAVE_DELTA);
	}

	if (game_state.pswitch > 0L) {
		--game_state.pswitch;
		if (game_state.pswitch == 100L)
			play_state_sound("starman", 0L, 0L, A_NULL);
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
				VAL(actor, SPROUT) -= FxOne;
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
			if (game_context.num_players > 1L) {
				GamePlayer* player = get_player(game_state.sequence.activator);
				if (player != NULL)
					hud_message(fmt("%s got the last kill!", get_player_name(player->id)));
			}
			game_state.sequence.type = SEQ_AMBUSH_END;
			break;
		}
	}

	case SEQ_NONE: {
		if (game_state.clock <= 0L || (game_state.time % 25L) != 0L)
			break;

		--game_state.clock;
		if (game_state.clock <= 100L && !(game_state.flags & GF_HURRY)) {
			play_state_sound("hurry", 0L, 0L, A_NULL);
			game_state.flags |= GF_HURRY;
		}

		if (game_state.clock <= 0L)
			for (PlayerID i = 0L; i < game_context.num_players; i++) {
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
		SDL_zeroa(game_state.next);
		game_state.flags |= GF_END;
		break;
	}

	case SEQ_WIN: {
		const Uint16 duration = (game_state.flags & GF_LOST) ? 150L : 400L;
		++game_state.sequence.time;

		if (game_state.sequence.time > duration && game_state.clock > 0L) {
			const Sint32 diff = (game_state.clock > 0L) + (((game_state.clock - 1L) >= 10L) * 10L);
			game_state.clock -= diff;

			GamePlayer* player = get_player(game_state.sequence.activator);
			if (player != NULL)
				player->score += diff * 10L;

			if ((game_state.time % 5L) == 0L)
				play_state_sound("tick", 0L, 0L, A_NULL);
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
		Uint32 score = 4294967295L; // World's most paranoid coder award
		for (PlayerID i = 0L; i < game_context.num_players; i++) {
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

	case SEQ_SECRET: {
		if (++game_state.sequence.time > 250L)
			game_state.flags |= GF_END;
		break;
	}
	}
}

#undef TICK_LOOP

static void save_game_state(GameState* gs) {
	*gs = game_state;
}

static void load_game_state(const GameState* gs) {
	game_state = *gs;

	// Fix for being stuck in spectator during rollback
	if (local_player == view_player)
		return;
	const GamePlayer* player = get_player(local_player);
	if (player != NULL && get_actor(player->actor) != NULL) {
		view_player = player->id;
		video_state.camera.lerp_time[1] = 0.f;
	}
}

static Uint32 check_game_state() {
	Uint32 checksum = 0;
	const Uint8* data = (Uint8*)(&game_state);
	for (Uint32 i = 0; i < sizeof(GameState); i++)
		checksum += data[i];
	return checksum;
}

static void dump_game_state() {
	INFO("====================");
	INFO("Flags: %u", game_state.flags);
	INFO("");
	INFO("[SEQUENCE]");
	INFO("	Type: %u", game_state.sequence.type);
	INFO("	Activator: %i", game_state.sequence.activator);
	INFO("	Time: %u", game_state.sequence.time);
	INFO("");
	INFO("[PLAYERS]");

	for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
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

		INFO("		Position: (%.2f, %.2f)", Fx2Float(player->pos.x), Fx2Float(player->pos.y));
		INFO("		Bounds: (%.2f, %.2f, %.2f, %.2f)", Fx2Float(player->bounds.start.x),
			Fx2Float(player->bounds.start.y), Fx2Float(player->bounds.end.x),
			Fx2Float(player->bounds.end.y));
		INFO("		Score: %u", player->score);
		INFO("		Kevin:");
		INFO("			Delay: %i", player->kevin.delay);
		INFO("			Actor: %i (%p)", player->kevin.actor, get_actor(player->kevin.actor));
		INFO("			Start: (%.2f, %.2f)", Fx2Float(player->kevin.start.x),
			Fx2Float(player->kevin.start.y));
		INFO("			(Frames...)");
		INFO("");
	}

	INFO("");
	INFO("Spawn: %i (%p)", game_state.spawn, get_actor(game_state.spawn));
	INFO("Checkpoint: %i (%p)", game_state.checkpoint, get_actor(game_state.checkpoint));
	INFO("Autoscroll: %i (%p)", game_state.autoscroll, get_actor(game_state.autoscroll));
	INFO("Wave: %i (%p)", game_state.wave, get_actor(game_state.wave));
	INFO("P-Switch: %u", game_state.pswitch);
	INFO("Size: (%.2f, %.2f)", Fx2Float(game_state.size.x), Fx2Float(game_state.size.y));
	INFO("Bounds: (%.2f, %.2f, %.2f, %.2f)", Fx2Float(game_state.bounds.start.x),
		Fx2Float(game_state.bounds.start.y), Fx2Float(game_state.bounds.end.x),
		Fx2Float(game_state.bounds.end.y));
	INFO("Water Y: %.2f", Fx2Float(game_state.water));
	INFO("Hazard Y: %.2f", Fx2Float(game_state.hazard));
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
		INFO("		Position: (%.2f, %.2f)", Fx2Float(actor->pos.x), Fx2Float(actor->pos.y));
		INFO("		Box: (%.2f, %.2f, %.2f, %.2f)", Fx2Float(actor->box.start.x),
			Fx2Float(actor->box.start.y), Fx2Float(actor->box.end.x), Fx2Float(actor->box.end.y));
		INFO("		Depth: %.2f", Fx2Float(actor->depth));
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
	INFO("	Next Level: %s", game_state.next);
	INFO("====================");
}

static void nuke_game_state() {
	clear_tilemaps();
	SDL_zero(game_state);

	for (PlayerID i = 0L; i < MAX_PLAYERS; i++) {
		game_state.players[i].id = NULLPLAY;
		game_state.players[i].actor = NULLACT;
		for (ActorID j = 0L; j < MAX_MISSILES; j++)
			game_state.players[i].missiles[j] = NULLACT;
		for (ActorID j = 0L; j < MAX_SINK; j++)
			game_state.players[i].sink[j] = NULLACT;
		game_state.players[i].kevin.actor = NULLACT;
	}

	game_state.live_actors = NULLACT;
	for (ActorID i = 0L; i < MAX_ACTORS; i++) {
		game_state.actors[i].id = game_state.actors[i].previous = game_state.actors[i].next
			= game_state.actors[i].previous_cell = game_state.actors[i].next_cell = NULLACT;
	}
	for (Sint32 i = 0L; i < GRID_SIZE; i++)
		game_state.grid[i] = NULLACT;

	game_state.size.x = game_state.bounds.end.x = F_SCREEN_WIDTH;
	game_state.size.y = game_state.bounds.end.y = F_SCREEN_HEIGHT;

	game_state.spawn = game_state.checkpoint = game_state.autoscroll = game_state.wave = NULLACT;
	game_state.water = FxFrom(32767L);

	game_state.clock = -1L;

	game_state.sequence.activator = NULLPLAY;

	SDL_zero(interp);
}

static void read_string(const char** buf, char* dest, Uint32 maxlen) {
	Uint32 i = 0;
	while (**buf != '\0') {
		if (i < maxlen)
			dest[i++] = **buf;
		*buf += 1;
	}
	*buf += 1;
	dest[SDL_min(i, maxlen - 1)] = '\0';
}

#define FLOAT_OFFS(idx) (*((float*)(buf + sizeof(float[(idx)]))))
#define BYTE_OFFS(idx) (*((Uint8*)(buf + sizeof(Uint8[(idx)]))))
static void start_game_state() {
	nuke_game_state();

	for (PlayerID i = 0L; i < game_context.num_players; i++)
		game_state.players[i].id = i;

	game_state.flags |= game_context.flags;
	if (game_context.num_players <= 1L)
		game_state.flags |= GF_SINGLE;
	else
		game_state.flags &= ~GF_SINGLE;

	game_state.checkpoint = game_context.checkpoint;

	//
	//
	//
	// DATA LOADER
	//
	//
	//
	load_texture_num("ui/coins%u", 3L);
	load_texture_num("markers/water%u", 8L);

	load_font("hud");
	load_font("main");

	load_sound("hurry");
	load_sound("tick");
	load_sound("pause");
	load_sound("switch");
	load_sound("select");
	load_sound("bump");
	load_sound("chat");

	//
	//
	//
	// LEVEL LOADER
	//
	//
	//
	const char* kla = find_data_file(fmt("data/levels/%s.*", game_context.level), NULL);
	if (kla == NULL) {
		show_error("Level \"%s\" not found", game_context.level);
		goto level_fail;
	}
	Uint8* data = SDL_LoadFile(kla, NULL);
	if (data == NULL) {
		show_error("Failed to load level \"%s\"\n%s", game_context.level, SDL_GetError());
		goto level_fail;
	}
	const Uint8* buf = data;

	// Header
	if (SDL_strncmp((const char*)buf, "Klawiatura", 10) != 0) {
		SDL_free(data);
		show_error("Invalid header in level \"%s\"", game_context.level);
		goto level_fail;
	}
	buf += 10;

	const Uint8 major = *buf;
	if (major != MAJOR_LEVEL_VERSION) {
		SDL_free(data);
		show_error("Invalid major version in level \"%s\"\nLevel: %u\nGame: %u)", game_context.level, major,
			MAJOR_LEVEL_VERSION);
		goto level_fail;
	}
	buf++;

	const Uint8 minor = *buf;
	if (minor < 1) {
		SDL_free(data);
		show_error("Invalid minor version in level \"%s\"\nLevel: %u\nGame: %u)", game_context.level, minor,
			MINOR_LEVEL_VERSION);
		goto level_fail;
	}
	buf++;

	// Level
	char level_name[32 + 1];
	SDL_memcpy(level_name, buf, 32);
	INFO("Level: %s (%s)", game_context.level, level_name);
	buf += 32;

	read_string((const char**)(&buf), video_state.world, sizeof(video_state.world));
	if (video_state.world[0] != ')' && video_state.world[0] != ']')
		load_texture(video_state.world);

	read_string((const char**)(&buf), (char*)game_state.next, sizeof(game_state.next));

	char track_name[3][GAME_STRING_MAX];
	for (int i = 0; i < 3; i++) {
		if (i == 2 && minor < 2)
			continue;
		read_string((const char**)(&buf), track_name[i], sizeof(track_name));
		load_track(track_name[i]);
	}

	game_state.flags |= *((GameFlag*)buf), buf += sizeof(GameFlag);
	if (game_state.flags & GF_AMBUSH)
		game_state.sequence.type = SEQ_AMBUSH;

	game_state.size.x = *((Fixed*)buf), buf += sizeof(Fixed);
	game_state.size.y = *((Fixed*)buf), buf += sizeof(Fixed);

	game_state.bounds.start.x = *((Fixed*)buf), buf += sizeof(Fixed);
	game_state.bounds.start.y = *((Fixed*)buf), buf += sizeof(Fixed);
	game_state.bounds.end.x = *((Fixed*)buf), buf += sizeof(Fixed);
	game_state.bounds.end.y = *((Fixed*)buf), buf += sizeof(Fixed);

	game_state.clock = *((Sint32*)buf), buf += sizeof(Sint32);
	game_state.water = *((Fixed*)buf), buf += sizeof(Fixed);
	game_state.hazard = *((Fixed*)buf), buf += sizeof(Fixed);

	// Markers
	Uint32 num_markers = *((Uint32*)buf);
	buf += sizeof(Uint32);

	for (Uint32 i = 0; i < num_markers; i++) {
		// Skip editor def
		while (*buf != '\0')
			buf++;
		buf++;

		Uint8 marker_type = *((Uint8*)buf);
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

			float rect[2][2] = {
				{FLOAT_OFFS(0), FLOAT_OFFS(1)},
                                {FLOAT_OFFS(2), FLOAT_OFFS(3)}
                        };
			buf += sizeof(float[2][2]);

			float z = *((float*)buf);
			buf += sizeof(float);

			Uint8 color[4][4] = {
				{BYTE_OFFS(0),  BYTE_OFFS(1),  BYTE_OFFS(2),  BYTE_OFFS(3) },
				{BYTE_OFFS(4),  BYTE_OFFS(5),  BYTE_OFFS(6),  BYTE_OFFS(7) },
				{BYTE_OFFS(8),  BYTE_OFFS(9),  BYTE_OFFS(10), BYTE_OFFS(11)},
				{BYTE_OFFS(12), BYTE_OFFS(13), BYTE_OFFS(14), BYTE_OFFS(15)}
                        };
			buf += sizeof(Uint8[4][4]);

			tile_rectangle(texture_name, rect, z, color);
			break;
		}

		case 2: { // Backdrop
			char texture_name[GAME_STRING_MAX];
			read_string((const char**)(&buf), texture_name, sizeof(texture_name));

			float pos[3] = {FLOAT_OFFS(0), FLOAT_OFFS(1), FLOAT_OFFS(2)};
			buf += sizeof(float[3]);

			float scale[2] = {FLOAT_OFFS(0), FLOAT_OFFS(1)};
			buf += sizeof(float[2]);

			Uint8 color[4] = {BYTE_OFFS(0), BYTE_OFFS(1), BYTE_OFFS(2), BYTE_OFFS(3)};
			buf += sizeof(Uint8[4]);

			tile_sprite(texture_name, pos, scale, color);
			break;
		}

		case 3: { // Object
			GameActorType type = *((GameActorType*)buf);
			buf += sizeof(GameActorType);

			FVec2 pos = {*((Fixed*)buf), *((Fixed*)(buf + sizeof(Fixed)))};
			buf += sizeof(FVec2);

			GameActor* actor = create_actor(type, pos);
			if (actor == NULL) {
				WARN("Unknown actor type %u", type);

				buf += sizeof(Fixed), buf += sizeof(FVec2);

				Uint8 num_values = *((Uint8*)buf);
				buf++;
				for (Uint8 j = 0; j < num_values; j++)
					buf += 1 + sizeof(ActorValue);
				buf += sizeof(ActorFlag);

				break;
			}

			actor->depth = *((Fixed*)buf), buf += sizeof(Fixed);

			FVec2 scale = {*((Fixed*)buf), *((Fixed*)(buf + sizeof(Fixed)))};
			buf += sizeof(FVec2);
			actor->box.start.x = Fmul(actor->box.start.x, scale.x);
			actor->box.start.y = Fmul(actor->box.start.y, scale.y);
			actor->box.end.x = Fmul(actor->box.end.x, scale.x);
			actor->box.end.y = Fmul(actor->box.end.y, scale.y);

			Uint8 num_values = *((Uint8*)buf);
			buf++;
			for (Uint8 j = 0; j < num_values; j++) {
				Uint8 idx = *((Uint8*)buf);
				buf++;
				actor->values[idx] = *((ActorValue*)buf), buf += sizeof(ActorValue);
			}
			FLAG_ON(actor, *((ActorFlag*)buf)), buf += sizeof(ActorFlag);

			switch (actor->type) {
			default:
				break;

			case ACT_HIDDEN_BLOCK: {
				if (ANY_FLAG(actor, FLG_BLOCK_ONCE) && (game_state.flags & GF_REPLAY))
					FLAG_ON(actor, FLG_DESTROY);
				break;
			}

			case ACT_WARP: {
				if (ANY_FLAG(actor, FLG_WARP_CALAMITY) && (game_state.flags & GF_REPLAY))
					FLAG_ON(actor, FLG_DESTROY);
				break;
			}

			case ACT_MUSHROOM:
			case ACT_MUSHROOM_1UP:
			case ACT_MUSHROOM_POISON: {
				if (ANY_FLAG(actor, FLG_POWERUP_CALAMITY) && (game_state.flags & GF_REPLAY))
					FLAG_ON(actor, FLG_DESTROY);
				break;
			}
			}

			load_actor(type);
			ACTOR_CALL(actor, load_special);
			break;
		}
		}
	}

	SDL_free(data);

	for (PlayerID i = 0L; i < MAX_PLAYERS; i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;

		player->lives = game_context.players[i].lives;
		player->coins = game_context.players[i].coins;
		player->score = game_context.players[i].score;
		player->power = game_context.players[i].power;
		respawn_player(player);
	}

	// !!! CLIENT-SIDE !!!
	if (get_player(local_player) == NULL)
		for (PlayerID i = 0L; i < MAX_PLAYERS; i++) {
			GamePlayer* player = get_player(i);
			if (player == NULL)
				continue;

			set_view_player(player);
			break;
		}
	// !!! CLIENT-SIDE !!!

	play_state_track(TS_MAIN, track_name[0], PLAY_LOOPING, 0L);

	update_discord_status(level_name);
	return;

level_fail:
	nuke_game_to_menu();
}
#undef FLOAT_OFFS
#undef BYTE_OFFS

void hud_message(const char* str) {
	if (rolling_back)
		return;
	SDL_strlcpy(video_state.message, str, sizeof(video_state.message));
	video_state.message_time = 0.f;
}

// =======
// PLAYERS
// =======

/// Fetch a player by its `PlayerID`.
GamePlayer* get_player(PlayerID id) {
	if (id < 0L || id >= game_context.num_players)
		return NULL;
	GamePlayer* player = &game_state.players[id];
	return (player->id != id) ? NULL : player;
}

/// Attempts to respawn the player.
GameActor* respawn_player(GamePlayer* player) {
	if (player == NULL || game_state.sequence.type == SEQ_WIN)
		return NULL;

	if (player->lives < 0L && !(game_state.flags & GF_HELL)) {
		// !!! CLIENT-SIDE !!!
		if (view_player == player->id)
			for (PlayerID i = 0L; i < game_context.num_players; i++) {
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
		player->bounds = game_state.bounds;

	pawn = create_actor(
		ACT_PLAYER, (spawn->type == ACT_AUTOSCROLL)
				    ? (FVec2){Flerp(game_state.bounds.start.x, game_state.bounds.end.x, FxHalf),
					      game_state.bounds.start.y}
				    : spawn->pos);
	if (pawn != NULL) {
		VAL(pawn, PLAYER_INDEX) = (ActorValue)player->id;
		player->actor = pawn->id;
		player->pos = player->kevin.start = pawn->pos;
		player->kevin.delay = 0L;

		FLAG_ON(pawn, spawn->flags & (FLG_X_FLIP | FLG_PLAYER_ASCEND | FLG_PLAYER_DESCEND));
		if (ANY_FLAG(pawn, FLG_PLAYER_ASCEND))
			VAL(pawn, Y_SPEED) -= FxFrom(22L);

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
GameActor* nearest_pawn(const FVec2 pos) {
	GameActor* nearest = NULL;
	Fixed dist = FxZero;

	for (PlayerID i = 0L; i < game_context.num_players; i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;
		GameActor* pawn = get_actor(player->actor);
		if (pawn == NULL)
			continue;

		const Fixed ndist = Vdist(pos, pawn->pos);
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
	return game_context.num_players;
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

const char* get_player_name(PlayerID pid) {
	return get_peer_name(player_to_peer(pid));
}

// ======
// ACTORS
// ======

/// Load an actor.
void load_actor(GameActorType type) {
	if (type < ACT_NULL || type >= ACT_SIZE)
		WARN("Loading invalid actor %u", type);
	else
		ACTOR_CALL_STATIC(type, load);
}

/// Create an actor.
GameActor* create_actor(GameActorType type, const FVec2 pos) {
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
	SDL_zerop(actor);

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
void move_actor(GameActor* actor, const FVec2 pos) {
	if (actor == NULL)
		return;

	actor->pos = pos;
	Sint32 cx = actor->pos.x / CELL_SIZE, cy = actor->pos.y / CELL_SIZE;
	cx = SDL_clamp(cx, 0L, MAX_CELLS - 1L), cy = SDL_clamp(cy, 0L, MAX_CELLS - 1L);

	const Sint32 cell = cx + (cy * MAX_CELLS);
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
		const FRect cbox = {Vsub(game_state.bounds.start, (FVec2){padding, padding}),                          \
			Vadd(game_state.bounds.end, (FVec2){padding, padding})};                                       \
		return (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y            \
			&& (ignore_top || abox.end.y > cbox.start.y));                                                 \
	}

Bool in_any_view(const GameActor* actor, Fixed padding, Bool ignore_top) {
	if (actor == NULL)
		return FALSE;

	const FRect abox = HITBOX(actor);

	CHECK_AUTOSCROLL_VIEW;

	for (PlayerID i = 0L; i < game_context.num_players; i++) {
		const GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;

		const Fixed cx = Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
				    player->bounds.end.x - F_HALF_SCREEN_WIDTH),
			    cy = Fclamp(player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT,
				    player->bounds.end.y - F_HALF_SCREEN_HEIGHT);

		const FRect cbox = {
			{cx - F_HALF_SCREEN_WIDTH - padding, cy - F_HALF_SCREEN_HEIGHT - padding},
			{cx + F_HALF_SCREEN_WIDTH + padding, cy + F_HALF_SCREEN_HEIGHT + padding}
                };
		if (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y
			&& (ignore_top || abox.end.y > cbox.start.y))
			return TRUE;
	}

	return FALSE;
}

/// Check if the actor is within a player's range.
Bool in_player_view(const GameActor* actor, GamePlayer* player, Fixed padding, Bool ignore_top) {
	if (actor == NULL || player == NULL)
		return FALSE;

	const FRect abox = HITBOX(actor);

	CHECK_AUTOSCROLL_VIEW;

	const Fixed cx = Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
			    player->bounds.end.x - F_HALF_SCREEN_WIDTH),
		    cy = Fclamp(player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT,
			    player->bounds.end.y - F_HALF_SCREEN_HEIGHT);

	const FRect cbox = {
		{cx - F_HALF_SCREEN_WIDTH - padding, cy - F_HALF_SCREEN_HEIGHT - padding},
		{cx + F_HALF_SCREEN_WIDTH + padding, cy + F_HALF_SCREEN_HEIGHT + padding}
        };
	return (abox.start.x < cbox.end.x && abox.end.x > cbox.start.x && abox.start.y < cbox.end.y
		&& (ignore_top || abox.end.y > cbox.start.y));
}

#undef CHECK_AUTOSCROLL_VIEW

/// Retrieve a list of actors overlapping a rectangle.
void list_cell_at(CellList* list, const FRect rect) {
	list->num_actors = 0L;

	Sint32 cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
	Sint32 cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L), cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L), cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (Sint32 cx = cx1; cx <= cx2; cx++)
		for (Sint32 cy = cy1; cy <= cy2; cy++)
			for (GameActor* actor = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]); actor != NULL;
				actor = get_actor(actor->previous_cell))
				if (!ANY_FLAG(actor, FLG_DESTROY) && Rcollide(rect, HITBOX(actor)))
					list->actors[list->num_actors++] = actor;
}

/// Check collision with other actors.
void collide_actor(GameActor* actor) {
	if (actor == NULL)
		return;

	const FRect abox = HITBOX(actor);
	Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
	Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L), cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L), cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (Sint32 cx = cx1; cx <= cx2; cx++)
		for (Sint32 cy = cy1; cy <= cy2; cy++)
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
Bool touching_solid(const FRect rect, SolidType types) {
	Sint32 cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
	Sint32 cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0L, MAX_CELLS - 1L), cy1 = SDL_clamp(cy1, 0L, MAX_CELLS - 1L);
	cx2 = SDL_clamp(cx2, 0L, MAX_CELLS - 1L), cy2 = SDL_clamp(cy2, 0L, MAX_CELLS - 1L);

	for (Sint32 cx = cx1; cx <= cx2; cx++)
		for (Sint32 cy = cy1; cy <= cy2; cy++)
			for (GameActor* actor = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]); actor != NULL;
				actor = get_actor(actor->previous_cell))
				if (ACTOR_IS_SOLID(actor, types) && actor->type != ACT_SOLID_SLOPE
					&& Rcollide(rect, HITBOX(actor)))
					return TRUE;

	return FALSE;
}

/// Move the actor with speed and displacement from solid actors.
void displace_actor(GameActor* actor, Fixed climb, Bool unstuck) {
	if (actor == NULL)
		return;

	if (unstuck
		&& touching_solid(
			(FRect){
				{actor->pos.x + actor->box.start.x, actor->pos.y + actor->box.start.y      },
				{actor->pos.x + actor->box.end.x,   actor->pos.y + actor->box.end.y - FxOne}
        },
			SOL_SOLID))
	{
		Fixed shift = ANY_FLAG(actor, FLG_X_FLIP) ? FxOne : -FxOne;
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
	Fixed x = actor->pos.x, y = actor->pos.y;

	// Horizontal collision
	x += VAL(actor, X_SPEED);
	CellList list = {0};
	list_cell_at(&list, (FRect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });
	Bool climbed = FALSE, stop = FALSE;

	const Bool right = VAL(actor, X_SPEED) >= FxZero;
	for (ActorID i = 0L; i < list.num_actors; i++) {
		GameActor* displacer = list.actors[i];
		if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID) || displacer->type == ACT_SOLID_SLOPE)
			continue;

		if (VAL(actor, Y_SPEED) >= FxZero
			&& (actor->pos.y + actor->box.end.y - climb) < (displacer->pos.y + displacer->box.start.y))
		{
			const Fixed step = displacer->pos.y + displacer->box.start.y - actor->box.end.y;
			const FRect solid = right ? (FRect){
				{x + actor->box.start.x + FxOne, step + actor->box.start.y - FxOne},
				{x + actor->box.end.x + FxOne,   step + actor->box.end.y - FxOne  }
			} : (FRect){
				{x + actor->box.start.x - FxOne, step + actor->box.start.y - FxOne},
				{x + actor->box.end.x - FxOne,   step + actor->box.end.y - FxOne  }
			};
			if (!touching_solid(solid, SOL_SOLID)) {
				y = step;
				VAL(actor, Y_SPEED) = FxZero;
				VAL(actor, Y_TOUCH) = 1L;
				climbed = TRUE;
			}
			continue;
		}

		if (right) {
			ACTOR_CALL2(displacer, on_left, actor);
			x = Fmin(x, displacer->pos.x + displacer->box.start.x - actor->box.end.x);
			stop |= VAL(actor, X_SPEED) >= FxZero;
		} else {
			ACTOR_CALL2(displacer, on_right, actor);
			x = Fmax(x, displacer->pos.x + displacer->box.end.x - actor->box.start.x);
			stop |= VAL(actor, X_SPEED) <= FxZero;
		}
		climbed = FALSE;
	}
	VAL(actor, X_TOUCH) = (right ? 1L : -1L) * (stop && !climbed);

	if (stop)
		VAL(actor, X_SPEED) = FxZero;

	// Vertical collision
	y += VAL(actor, Y_SPEED);
	list_cell_at(&list, (FRect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		Bool stop = FALSE;
		if (VAL(actor, Y_SPEED) < FxZero) {
			for (ActorID i = 0L; i < list.num_actors; i++) {
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
			VAL(actor, Y_TOUCH) = -(Fixed)stop;
		} else if (VAL(actor, Y_SPEED) > FxZero) {
			for (ActorID i = 0L; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer)
					continue;

				if (displacer->type == ACT_SOLID_SLOPE) {
					const Bool side = ANY_FLAG(displacer, FLG_X_FLIP);

					const Fixed sa = displacer->pos.y
					                 + (side ? displacer->box.start.y : displacer->box.end.y),
						    sb = displacer->pos.y
					                 + (side ? displacer->box.end.y : displacer->box.start.y);

					const Fixed ax = x + (side ? actor->box.start.x : actor->box.end.x),
						    sx = displacer->pos.x
					                 + (side ? displacer->box.end.x : displacer->box.start.x);

					const Fixed slope = Flerp(sa, sb,
						Fclamp(Fdiv(ax - sx, displacer->box.end.x - displacer->box.start.x),
							FxZero, FxOne));

					if (y >= slope) {
						y = slope;
						stop = TRUE;
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
			VAL(actor, Y_TOUCH) = (Fixed)stop;
		}

		if (stop)
			VAL(actor, Y_SPEED) = FxZero;
	}

	move_actor(actor, (FVec2){x, y});
}

/// Move the actor with speed and touch solid actors without being pushed away.
// FIXME: Implement slope collisions for this variant.
void displace_actor_soft(GameActor* actor) {
	if (actor == NULL)
		return;

	VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0L;
	Fixed x = actor->pos.x, y = actor->pos.y;

	// Horizontal collision
	x += VAL(actor, X_SPEED);
	CellList list = {0};
	list_cell_at(&list, (FRect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		if (VAL(actor, X_SPEED) < FxZero)
			for (ActorID i = 0L; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_right, actor);
				VAL(actor, X_TOUCH) = -1L;
			}
		else if (VAL(actor, X_SPEED) > FxZero)
			for (ActorID i = 0L; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_left, actor);
				VAL(actor, X_TOUCH) = 1L;
			}
	}

	// Vertical collision
	y += VAL(actor, Y_SPEED);
	list_cell_at(&list, (FRect){
				    {x + actor->box.start.x, y + actor->box.start.y},
				    {x + actor->box.end.x,   y + actor->box.end.y  }
        });

	if (list.num_actors > 0L) {
		if (VAL(actor, Y_SPEED) < FxZero)
			for (ActorID i = 0L; i < list.num_actors; i++) {
				GameActor* displacer = list.actors[i];
				if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID))
					continue;

				ACTOR_CALL2(displacer, on_bottom, actor);
				VAL(actor, Y_TOUCH) = -1L;
			}
		else if (VAL(actor, Y_SPEED) > FxZero)
			for (ActorID i = 0L; i < list.num_actors; i++) {
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

	move_actor(actor, (FVec2){x, y});
}

/// Encodes a 16-character string into an actor.
void encode_actor_string(GameActor* actor, ActorValue offset, const Sint8 src[ACTOR_STRING_MAX]) {
	if (actor == NULL)
		return;
	ActorValue* dst = actor->values + offset;
	for (ActorValue i = 0L; i < 4L; i++) {
		const ActorValue j = i * 4L;
		dst[i] = ((ActorValue)(Sint8)src[j + 0L]) | ((ActorValue)(Sint8)src[j + 1L] << 8L)
		         | ((ActorValue)(Sint8)src[j + 2L] << 16L) | ((ActorValue)(Sint8)src[j + 3L] << 24L);
	}
}

/// Decodes a 16-character string from an actor.
void decode_actor_string(const GameActor* actor, ActorValue offset, Sint8 dst[ACTOR_STRING_MAX]) {
	if (actor == NULL)
		return;
	const ActorValue* src = actor->values + offset;
	for (ActorValue i = 0L; i < 4L; i++) {
		const ActorValue j = i * 4L, k = (ActorValue)src[i];
		dst[j + 0L] = (Sint8)(k & 255L);
		dst[j + 1L] = (Sint8)((k >> 8L) & 255L);
		dst[j + 2L] = (Sint8)((k >> 16L) & 255L);
		dst[j + 3L] = (Sint8)((k >> 24L) & 255L);
	}
}

/// Generic interpolated actor drawing function.
//
// Formula for current static actor frame: `(game_state.time / ((TICKRATE * 2) / speed)) % frames`
void draw_actor(const GameActor* actor, const char* name, float angle, const Uint8 color[4]) {
	draw_actor_offset(actor, name, B_ORIGIN, angle, color);
}

// Variant of `draw_actor()` with an offset.
void draw_actor_offset(
	const GameActor* actor, const char* name, const float offs[3], float angle, const Uint8 color[4]) {
	const InterpActor* iactor = get_interp(actor);
	const float sprout = VAL(actor, SPROUT), z = (sprout > FxZero) ? 21.f : Fx2Float(actor->depth);
	batch_reset();
	batch_pos(B_XYZ(Fx2Int(iactor->pos.x) + offs[0], Fx2Int(iactor->pos.y + sprout) + offs[1], z + offs[2]));
	batch_angle(angle), batch_color(color);
	batch_flip(B_FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
	batch_sprite(name);
}

// Variant of `draw_actor()` that works around some jittering issues (i.e. players on platforms, autoscrolling).
void draw_actor_no_jitter(const GameActor* actor, const char* name, float angle, const Uint8 color[4]) {
	if (actor->type == ACT_PLAYER && get_actor(VAL(actor, PLAYER_PLATFORM)) == NULL) {
		const GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll == NULL || !ANY_FLAG(autoscroll, FLG_SCROLL_TANKS)
			|| (VAL(actor, PLAYER_GROUND) > 0L && actor->pos.y < (autoscroll->pos.y + FxFrom(415L))))
		{
			draw_actor(actor, name, angle, color);
			return;
		}
	}

	const InterpActor* iactor = get_interp(actor);
	const float sprout = Fx2Float(VAL(actor, SPROUT)), z = sprout > 0.f ? 21.f : Fx2Float(actor->depth);
	batch_reset();
	batch_pos(B_XYZ((int)(Fx2Float(iactor->pos.x) - camera_offset_morsel[0]),
		(int)(Fx2Float(iactor->pos.y) + sprout + camera_offset_morsel[1]), z));
	batch_angle(angle), batch_color(color);
	batch_flip(B_FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
	batch_sprite(name);
}

void draw_dead(const GameActor* actor) {
	const GameActorType type = VAL(actor, DEAD_TYPE);
	if (ACTORS[type] != NULL && ACTORS[type]->draw_dead != NULL)
		ACTORS[type]->draw_dead(actor);
}

/// Convenience function for quaking at an actor's position.
void quake_at_actor(const GameActor* actor, float amount) {
	const float dist = glm_vec2_distance(video_state.camera.pos, (vec2){Fx2Int(actor->pos.x), Fx2Int(actor->pos.y)})
	                   / (float)SCREEN_HEIGHT;
	const float quake = amount / SDL_max(dist, 1.f);
	video_state.quake = SDL_min(video_state.quake + quake, 10.f);
}

// ====
// MATH
// ====

/// Produce an exclusive random number.
// https://rosettacode.org/wiki/Linear_congruential_generator
Sint32 rng(Sint32 x) {
	if (x <= 0L)
		return 0L;
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
	iactor->from = iactor->pos = actor->pos;
}

/// Start interpolating an actor's position from another actor. Used for trails.
void align_interp(const GameActor* actor, const GameActor* from) {
	if (BAD_ACTOR(actor) || BAD_ACTOR(from))
		return;
	InterpActor *iactor = &interp.actors[actor->id], *ifrom = &interp.actors[from->id];
	iactor->from = ifrom->from, iactor->pos = ifrom->pos;
}

#undef BAD_ACTOR
