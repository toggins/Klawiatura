#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"

enum {
    VAL_CORAL_FRAME,
    VAL_CORAL_OVERLAP
};

static void load() {
    load_sprite_num("enemies/coral/%u", 22, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.end.x = Int2Fx(28);
    actor->box.end.y = Int2Fx(30);

    actor->depth = 1310719;
}

static void tick(GameActor* actor) {
    ++VAL(actor, CORAL_FRAME);
    VAL_TICK(actor, CORAL_OVERLAP);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/coral/%i", (VAL(actor, CORAL_FRAME) / 2) % 22), FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER)
        return;

    if (VAL(actor, CORAL_OVERLAP) > 0) {
        VAL(actor, CORAL_OVERLAP) = 2;
        return;
    }

    VAL(actor, CORAL_OVERLAP) = 2;
    hit_player(from);
}

const ActorTable TAB_ELECTRIC_CORAL = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};
