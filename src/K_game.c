#include "K_game.h"

const GameActorTable TAB_NULL = {0};
extern const GameActorTable TAB_PLAYER;

static const GameActorTable* const ACTORS[ACT_SIZE] = {
	[ACT_NULL] = &TAB_NULL,
	[ACT_PLAYER] = &TAB_PLAYER,
};

GameState game_state = {0};

// =======
// PLAYERS
// =======

// Gets a player from PlayerID.
GamePlayer* get_player(PlayerID id) {
	if (id < 0 || id >= MAX_PLAYERS)
		return NULL;

	GamePlayer* player = &game_state.players[id];
	return (player->id == NULLPLAY) ? NULL : player;
}

// ======
// ACTORS
// ======

// Gets an actor from ActorID.
GameActor* get_actor(ActorID id) {
	if (id < 0 || id >= MAX_ACTORS)
		return NULL;

	GameActor* actor = &game_state.actors[id];
	return (actor->id == NULLACT) ? NULL : actor;
}

// ====
// MATH
// ====

// Returns an exclusive random number.
int32_t rng(int32_t x) {
	// https://rosettacode.org/wiki/Linear_congruential_generator
	game_state.seed = (game_state.seed * 1103515245 + 12345) & 2147483647;
	return game_state.seed % x;
}

// Approximate distance between two points.
// Pythagorean theorem is useless in fixed point math due to overflows.
fix16_t point_distance(const fvec2 a, const fvec2 b) {
	const fix16_t dx = Fabs(Fsub(b[0], a[0]));
	const fix16_t dy = Fabs(Fsub(b[1], a[1]));
	return Fsub(Fadd(dx, dy), Fhalf(Fmin(dx, dy)));
}

// Radian angle between two points.
fix16_t point_angle(const fvec2 a, const fvec2 b) {
	return Fatan2(Fsub(b[1], a[1]), Fsub(b[0], a[0]));
}
