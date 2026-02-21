#include "K_game.h"

#define DUMMY_ACTOR ACT_KOOPA

enum {
	VAL_DEATH_TIMER = MAX_VALUES - 1L,
};

static void load() {
	load_actor(DUMMY_ACTOR);
}

static void tick(GameActor* self) {
	for (GameActor* other = get_actor(game_state.live_actors); other != NULL; other = get_actor(other->previous))
		if (other->type == DUMMY_ACTOR && VAL(other, DEATH_TIMER)++ >= 600L)
			FLAG_ON(other, FLG_DESTROY);
	GameActor* dummy = create_actor(DUMMY_ACTOR, self->pos);
	VAL(dummy, DEATH_TIMER) = 0L; // redundant but sure...
}

const GameActorTable TAB_BENCHMARK = {.load = load, .tick = tick};
