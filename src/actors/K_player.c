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

	fvec2 move = {FfInt(((int8_t)ANY_INPUT(player, GI_RIGHT) - (int8_t)ANY_INPUT(player, GI_LEFT)) * 2L),
		FfInt(((int8_t)ANY_INPUT(player, GI_DOWN) - (int8_t)ANY_INPUT(player, GI_UP)) * 2L)};
	if (!touching_solid(HITBOX_ADD(actor, move.x, move.y), SOL_ALL))
		move_actor(actor, POS_ADD(actor, move.x, move.y));
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
