#include "K_game.h"
#include "K_log.h"

#define ACTOR_TYPE_CALL(type, fn)                                                                                      \
	do {                                                                                                           \
		if (ACTORS[(type)]->fn != NULL)                                                                        \
			ACTORS[(type)]->fn();                                                                          \
	} while (0)

#define ACTOR_CALL(act, fn)                                                                                            \
	do {                                                                                                           \
		if (ACTORS[(act)->type]->fn != NULL)                                                                   \
			ACTORS[(act)->type]->fn((act));                                                                \
	} while (0)

#define ACTOR_CALL_DUO(act, fn, act2)                                                                                  \
	do {                                                                                                           \
		if (ACTORS[(act)->type]->fn != NULL)                                                                   \
			ACTORS[(act)->type]->fn((act), (act2));                                                        \
	} while (0)

const GameActorTable TAB_NULL = {0};
extern const GameActorTable TAB_PLAYER;

static const GameActorTable* const ACTORS[ACT_SIZE] = {
	[ACT_NULL] = &TAB_NULL,
	[ACT_PLAYER] = &TAB_PLAYER,
};

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;
GameState game_state = {0};

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
	cfg.state_size = sizeof(GameState);
	cfg.max_spectators = 0;
	cfg.input_prediction_window = 4;
	cfg.num_players = ctx->num_players;

	populate_game(game_session);
	gekko_start(game_session, &cfg);

	start_game_state(ctx);
	start_audio_state();
	start_video_state();
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

void update_game() {}

void draw_game() {
	start_drawing();
	batch_cursor(0, 0, 0);
	batch_color(WHITE);
	batch_sprite("ui/sidebar_l", NO_FLIP);
	batch_sprite("ui/sidebar_r", NO_FLIP);

	push_surface(game_surface);
	clear_color(0, 0, 0, 1);
	clear_depth(1);
	pop_surface();

	batch_cursor(0, 0, 0);
	batch_color(WHITE);
	batch_surface(game_surface);

	stop_drawing();
}

void nuke_game_state() {
	SDL_memset(&game_state, 0, sizeof(game_state));

	game_state.live_actors = NULLACT;
	for (int32_t i = 0; i < GRID_SIZE; i++)
		game_state.grid[i] = NULLACT;

	game_state.size.x = game_state.bounds.end.x = F_SCREEN_WIDTH;
	game_state.size.y = game_state.bounds.end.y = F_SCREEN_HEIGHT;

	game_state.spawn = game_state.checkpoint = game_state.autoscroll = NULLACT;
	game_state.water = FfInt(32767);

	game_state.clock = -1;

	game_state.sequence.activator = NULLPLAY;
}

void start_game_state(GameContext* ctx) {
	nuke_game_state();
}

// =======
// PLAYERS
// =======

/// Fetch a player by its `PlayerID`.
GamePlayer* get_player(PlayerID id) {
	if (id < 0 || id >= MAX_PLAYERS)
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
	for (ActorID i = 0; i < MAX_ACTORS; i++) {
		GameActor* actor = &game_state.actors[index];
		if (actor->id == NULLACT) {
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

			game_state.next_actor = (ActorID)((index + 1) % MAX_ACTORS);
			return actor;
		}
		index = (ActorID)((index + 1) % MAX_ACTORS);
	}

	return NULL;
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
	if (cell >= 0 && cell < GRID_SIZE) {
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
	if (id < 0 || id >= MAX_ACTORS)
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
	cx = SDL_clamp(cx, 0, MAX_CELLS - 1), cy = SDL_clamp(cy, 0, MAX_CELLS - 1);

	const int32_t cell = (cy * MAX_CELLS) + cx;
	if (cell == actor->cell)
		return;

	// Unlink old cell
	if (cell >= 0 && cell < GRID_SIZE) {
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
	list->num_actors = 0;

	int32_t cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE;
	int32_t cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
	int32_t cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE;
	int32_t cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
	cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
	cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
	cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
	cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

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

// ====
// MATH
// ====

/// Produce an exclusive random number.
int32_t rng(int32_t x) {
	// https://rosettacode.org/wiki/Linear_congruential_generator
	game_state.seed = (game_state.seed * 1103515245 + 12345) & 2147483647;
	return game_state.seed % x;
}
