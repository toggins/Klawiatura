#include "K_game.h"

static void create(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

const ActorTable TAB_SOLID = {.is_solid = always_solid, .create = create},
                 TAB_SOLID_TOP = {.is_solid = always_top, .create = create},
                 TAB_SOLID_SLOPE = {.is_solid = always_solid_slope, .create = create};
