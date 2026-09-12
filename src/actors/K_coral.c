#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"

static void load() {
    load_sprite_num("enemies/coral/%u", 22, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.end.x = Int2Fx(28);
    actor->box.end.y = Int2Fx(30);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/coral/%i", (gamestate()->time / 2) % 22), FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    (void)actor;

    hit_player(from);
}

const ActorTable TAB_ELECTRIC_CORAL = {
    .load = load,
    .create = create,
    .draw = draw,
    .collide = collide,
};
