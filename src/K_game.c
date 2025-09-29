#include "K_file.h"
#include "K_game.h"
#include "K_input.h"
#include "K_log.h"
#include "K_string.h"
#include "K_tick.h"

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

const GameActorTable TAB_NULL = {0};
extern const GameActorTable TAB_PLAYER;
extern const GameActorTable TAB_CLOUD;
extern const GameActorTable TAB_BUSH;

static const GameActorTable* const ACTORS[ACT_SIZE] = {
	[ACT_NULL] = &TAB_NULL,
	[ACT_PLAYER] = &TAB_PLAYER,
	[ACT_CLOUD] = &TAB_CLOUD,
	[ACT_BUSH] = &TAB_BUSH,
};

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;
GameState game_state = {0};

PlayerID local_player = -1L, view_player = -1L, num_players = 0L;

// ====
// GAME
// ====

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
	cfg.input_prediction_window = 4;
	cfg.num_players = num_players = ctx->num_players;
	local_player = view_player = ctx->local_player;

	populate_game(game_session);
	gekko_start(game_session, &cfg);
	gekko_net_adapter_set(game_session, NULL); // FIXME: Write a NutPunch adapter

	start_game_state(ctx);
	start_audio_state();
	start_video_state();
	from_scratch();
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

	gekko_destroy(game_session);
	game_session = NULL;
	destroy_surface(game_surface);
	game_surface = NULL;
}

bool update_game() {
	if (num_players > 1)
		goto rollbacker;

	for (new_frame(0); got_ticks(); next_tick()) {
		if (kb_pressed(KB_PAUSE))
			goto byebye_game;

		GameInput inputs[MAX_PLAYERS] = {0};
		if (kb_pressed(KB_UP))
			inputs[0] |= GI_UP;
		if (kb_pressed(KB_LEFT))
			inputs[0] |= GI_LEFT;
		if (kb_pressed(KB_DOWN))
			inputs[0] |= GI_DOWN;
		if (kb_pressed(KB_RIGHT))
			inputs[0] |= GI_RIGHT;
		if (kb_pressed(KB_JUMP))
			inputs[0] |= GI_JUMP;
		if (kb_pressed(KB_FIRE))
			inputs[0] |= GI_FIRE;
		if (kb_pressed(KB_RUN))
			inputs[0] |= GI_RUN;

		tick_game_state(inputs);
		tick_video_state();
		tick_audio_state();
	}

	return true;

rollbacker:
	gekko_network_poll(game_session);

	const float ahead = gekko_frames_ahead(game_session);
	for (new_frame(SDL_clamp(ahead, 0, 2)); got_ticks(); next_tick()) {
		if (kb_pressed(KB_PAUSE))
			goto byebye_game;

		GameInput input = 0;
		if (kb_pressed(KB_UP))
			input |= GI_UP;
		if (kb_pressed(KB_LEFT))
			input |= GI_LEFT;
		if (kb_pressed(KB_DOWN))
			input |= GI_DOWN;
		if (kb_pressed(KB_RIGHT))
			input |= GI_RIGHT;
		if (kb_pressed(KB_JUMP))
			input |= GI_JUMP;
		if (kb_pressed(KB_FIRE))
			input |= GI_FIRE;
		if (kb_pressed(KB_RUN))
			input |= GI_RUN;

		gekko_add_local_input(game_session, local_player, &input);

		int count = 0;
		GekkoSessionEvent** events = gekko_session_events(game_session, &count);
		for (int i = 0; i < count; i++) {
			GekkoSessionEvent* event = events[i];
			switch (event->type) {
			case DesyncDetected: {
				dump_game_state();
				struct Desynced desync = event->data.desynced;
				INFO("OOPS: Out of sync with player %d (tick %d, l %u != r %u)",
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
				INFO("OOPS: Player %i disconnected", disconnect.handle + 1);
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
				save_video_state(&(save.video));
				save_audio_state(&(save.audio));

				*(event->data.save.state_len) = sizeof(save);
				*(event->data.save.checksum) = check_game_state();
				SDL_memcpy(event->data.save.state, &save, sizeof(save));
				break;
			}

			case LoadEvent: {
				const SaveState* load = (SaveState*)(event->data.load.state);
				load_game_state(&(load->game));
				load_video_state(&(load->video));
				load_audio_state(&(load->audio));
				break;
			}

			case AdvanceEvent: {
				GameInput inputs[MAX_PLAYERS] = {0};
				for (size_t j = 0; j < num_players; j++)
					inputs[j] = ((GameInput*)(event->data.adv.inputs))[j];
				tick_game_state(inputs);
				tick_video_state();
				tick_audio_state();
				break;
			}

			default:
				break;
			}
		}
	}

	return true;

byebye_game:
	nuke_game();
	return false;
}

void draw_game() {
	if (game_session == NULL)
		return;

	start_drawing();
	batch_start(ORIGO, 0, WHITE);
	batch_sprite("ui/sidebar_l", NO_FLIP);
	batch_sprite("ui/sidebar_r", NO_FLIP);

	push_surface(game_surface);

	// clear_color(0, 0, 0, 1);
	clear_depth(1);

	draw_tilemaps();

	const GameActor* actor = get_actor(game_state.live_actors);
	while (actor != NULL) {
		if (ANY_FLAG(actor, FLG_VISIBLE))
			ACTOR_CALL(actor, draw);
		actor = get_actor(actor->previous);
	}

	pop_surface();

	batch_start(ORIGO, 0, WHITE);
	batch_surface(game_surface);

	stop_drawing();
}

static void destroy_actor(GameActor*);
void tick_game_state(const GameInput inputs[MAX_PLAYERS]) {
	GameActor* actor = get_actor(game_state.live_actors);
	while (actor != NULL) {
		ACTOR_CALL(actor, tick);

		GameActor* next = get_actor(actor->previous);
		if (ANY_FLAG(actor, FLG_DESTROY))
			destroy_actor(actor);
		actor = next;
	}

	++game_state.time;
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
	for (size_t i = 0; i < sizeof(GameState); i++)
		checksum += data[i];
	return checksum;
}

void dump_game_state() {}

void nuke_game_state() {
	clear_tilemaps();
	SDL_memset(&game_state, 0, sizeof(game_state));

	game_state.live_actors = NULLACT;
	for (ActorID i = 0; i < MAX_ACTORS; i++)
		game_state.actors[i].id = NULLACT;
	for (int32_t i = 0; i < GRID_SIZE; i++)
		game_state.grid[i] = NULLACT;

	game_state.size.x = game_state.bounds.end.x = F_SCREEN_WIDTH;
	game_state.size.y = game_state.bounds.end.y = F_SCREEN_HEIGHT;

	game_state.spawn = game_state.checkpoint = game_state.autoscroll = NULLACT;
	game_state.water = FfInt(32767);

	game_state.clock = -1;

	game_state.sequence.activator = NULLPLAY;
}

static void read_string(const char** buf, char* dest, size_t maxlen) {
	size_t i = 0;
	while (**buf != '\0') {
		if (i < maxlen)
			dest[i++] = **buf;
		*buf += sizeof(char);
	}
	*buf += sizeof(char);
	dest[SDL_min(i, maxlen - 1)] = '\0';
}

#define FLOAT_OFFS(idx) (*((GLfloat*)(buf + sizeof(GLfloat[(idx)]))))
#define BYTE_OFFS(idx) (*((GLubyte*)(buf + sizeof(GLubyte[(idx)]))))
void start_game_state(GameContext* ctx) {
	nuke_game_state();

	game_state.flags |= ctx->flags;
	game_state.checkpoint = ctx->checkpoint;

	//
	//
	//
	// DATA LOADER
	//
	//
	//
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
	if (data == NULL) {
		WTF("Failed to load level \"%s\": %s", ctx->level, SDL_GetError());
		return;
	}
	const uint8_t* buf = data;

	// Header
	if (SDL_strncmp((const char*)buf, "Klawiatura", sizeof(char[10])) != 0) {
		WTF("Invalid level header");
		SDL_free(data);
		return;
	}
	buf += sizeof(char[10]);

	const uint8_t major = *buf;
	if (major != MAJOR_LEVEL_VERSION) {
		WTF("Invalid major version (%u != %u)", major, MAJOR_LEVEL_VERSION);
		SDL_free(data);
		return;
	}
	buf += sizeof(uint8_t); // MAJOR_LEVEL_VERSION

	const uint8_t minor = *buf;
	if (minor != MINOR_LEVEL_VERSION) {
		WTF("Invalid minor version (%u != %u)", minor, MINOR_LEVEL_VERSION);
		SDL_free(data);
		return;
	}
	buf += sizeof(uint8_t); // MINOR_LEVEL_VERSION

	// Level
	char level_name[32];
	SDL_memcpy(level_name, buf, sizeof(char[32]));
	INFO("Level: %s (%s)", ctx->level, level_name);
	buf += sizeof(char[32]);

	read_string((const char**)(&buf), game_state.world, sizeof(game_state.world));
	load_texture(game_state.world);

	read_string((const char**)(&buf), game_state.next, sizeof(game_state.next));

	char track_name[2][GAME_STRING_MAX];
	read_string((const char**)(&buf), track_name[0], sizeof(track_name));
	read_string((const char**)(&buf), track_name[1], sizeof(track_name));

	game_state.flags |= *((GameFlag*)buf);
	buf += sizeof(GameFlag);

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
			buf += sizeof(char);
		buf += sizeof(char);

		uint8_t marker_type = *((uint8_t*)buf);
		buf += sizeof(uint8_t);

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
				WARN("Unknown object type %u", type);

				buf += sizeof(fixed);
				buf += sizeof(fvec2);

				uint8_t num_values = *((uint8_t*)buf);
				buf += sizeof(uint8_t);

				for (uint8_t j = 0; j < num_values; j++) {
					buf += sizeof(uint8_t);
					buf += sizeof(ActorValue);
				}

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
			buf += sizeof(uint8_t);

			for (uint8_t j = 0; j < num_values; j++) {
				uint8_t idx = *((uint8_t*)buf);
				buf += sizeof(uint8_t);

				actor->values[idx] = *((ActorValue*)buf);
				buf += sizeof(ActorValue);
			}

			actor->flags |= *((ActorFlag*)buf);
			buf += sizeof(ActorFlag);

			// if (actor->type == ACT_HIDDEN_BLOCK && (object->flags & FLG_BLOCK_ONCE)
			// 	&& (game_state.flags & GF_REPLAY))
			// 	actor->flags |= FLG_DESTROY;
			break;
		}
		}
	}

	SDL_free(data);
}
#undef FLOAT_OFFS
#undef BYTE_OFFS

// =======
// PLAYERS
// =======

/// Fetch a player by its `PlayerID`.
GamePlayer* get_player(PlayerID id) {
	if (id < 0L || id >= MAX_PLAYERS)
		return NULL;
	GamePlayer* player = &game_state.players[id];
	return (player->id == NULLPLAY) ? NULL : player;
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

	FLAG_ON(actor, FLG_VISIBLE);
	ACTOR_CALL(actor, create);

	game_state.next_actor = (ActorID)((index + 1L) % MAX_ACTORS);
	return actor;
}

/// (INTERNAL) Nuke an actor.
static void destroy_actor(GameActor* actor) {
	if (actor == NULL)
		return;

	const GameActorType type = actor->type;
	if (type <= ACT_NULL || type >= ACT_SIZE)
		WARN("Destroying invalid actor %u", type);
	else
		ACTOR_CALL(actor, cleanup);

	actor->id = NULLACT;
	actor->type = ACT_NULL;

	// Unlink cell
	const int32_t cell = actor->cell;
	if (cell >= 0L && cell < GRID_SIZE) {
		GameActor* neighbor = get_actor(actor->previous_cell);
		if (neighbor != NULL)
			neighbor->next_cell = actor->next_cell;
		actor->next_cell = NULLACT;

		neighbor = get_actor(actor->next_cell);
		if (neighbor != NULL)
			neighbor->previous_cell = actor->previous_cell;
		actor->previous_cell = NULLACT;

		if (game_state.grid[cell] == actor->id)
			game_state.grid[cell] = actor->previous_cell;
	}
	actor->cell = NULLCELL;

	// Unlink list
	if (game_state.live_actors == actor->id)
		game_state.live_actors = actor->previous;

	GameActor* neighbor = get_actor(actor->previous);
	if (neighbor != NULL)
		neighbor->next = actor->next;

	neighbor = get_actor(actor->next);
	if (neighbor != NULL)
		neighbor->previous = actor->previous;
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

	const int32_t cell = (cy * MAX_CELLS) + cx;
	if (cell == actor->cell)
		return;

	// Unlink old cell
	if (cell >= 0L && cell < GRID_SIZE) {
		GameActor* neighbor = get_actor(actor->previous_cell);
		if (neighbor != NULL)
			neighbor->next_cell = actor->next_cell;

		neighbor = get_actor(actor->next_cell);
		if (neighbor != NULL)
			neighbor->previous_cell = actor->previous_cell;

		if (game_state.grid[cell] == actor->id)
			game_state.grid[cell] = actor->previous_cell;
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

	for (int32_t cy = cy1; cy <= cy2; cy++)
		for (int32_t cx = cx1; cx <= cx2; cx++) {
			GameActor* actor = get_actor(game_state.grid[cx + (cy * MAX_CELLS)]);
			while (actor != NULL) {
				if (ANY_FLAG(actor, FLG_DESTROY))
					goto next;
				frect abox;
				abox.start = Vadd(actor->pos, actor->box.start);
				abox.end = Vadd(actor->pos, actor->box.end);
				if (Rcollide(rect, abox))
					list->actors[list->num_actors++] = actor;
			next:
				actor = get_actor(actor->previous_cell);
			}
		}
}

// Generic interpolated (not yet) actor drawing function.
//
// Formula for current static actor frame: `(game_state.time / ((TICKRATE * 2) / speed)) % frames`
void draw_actor(const GameActor* actor, const char* name, GLfloat angle, const GLubyte color[4]) {
	batch_start(XYZ(FtFloat(actor->pos.x), FtFloat(actor->pos.y), FtFloat(actor->depth)), angle, color);
	batch_sprite(name, FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
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
