#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"

static void load() {
    load_sprite_num("enemies/lava/%u", 7, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.y = Int2Fx(-17);
    actor->box.end.x = Int2Fx(32);
    actor->box.end.y = Int2Fx(31);

    actor->depth = Int2Fx(20);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/lava/%i", ((gamestate()->time * 11) / 100) % 7), FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    (void)actor;

    if (from->type == ACT_PLAYER)
        kill_player(from);
}

const ActorTable TAB_LAVA = {
    .load = load,
    .create = create,
    .draw = draw,
    .collide = collide,
};
