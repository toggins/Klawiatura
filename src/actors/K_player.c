#include "K_player.h"

enum {
	FLG_PLAYER_DUCK = CUSTOM_FLAG(0),
	FLG_PLAYER_JUMP = CUSTOM_FLAG(1),
	FLG_PLAYER_SWIM = CUSTOM_FLAG(2),
	FLG_PLAYER_ASCEND = CUSTOM_FLAG(3),
	FLG_PLAYER_DESCEND = CUSTOM_FLAG(4),
	FLG_PLAYER_RESPAWN = CUSTOM_FLAG(5),
	FLG_PLAYER_STOMP = CUSTOM_FLAG(6),
	FLG_PLAYER_WARP_OUT = CUSTOM_FLAG(7),
	FLG_PLAYER_DEAD = CUSTOM_FLAG(8),
};

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
}

static void create(GameActor* actor) {}

static void tick(GameActor* actor) {
	GamePlayer* player = get_player((PlayerID)actor->values[VAL_PLAYER_INDEX]);
	if (player == NULL) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	move_actor(actor,
		(fvec2){actor->pos.x + FfInt((int8_t)ANY_INPUT(player, GI_RIGHT) - (int8_t)ANY_INPUT(player, GI_LEFT)),
			actor->pos.y + FfInt((int8_t)ANY_INPUT(player, GI_DOWN) - (int8_t)ANY_INPUT(player, GI_UP))});
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

const GameActorTable TAB_PLAYER = {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup};
