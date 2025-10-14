#include "K_game.h"

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

const GameActorTable TAB_SOLID = {.is_solid = always_solid, .create = create};
const GameActorTable TAB_SOLID_TOP = {.is_solid = always_top, .create = create};
const GameActorTable TAB_SOLID_SLOPE = {.is_solid = always_solid, .create = create};
