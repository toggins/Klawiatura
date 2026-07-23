#include "K_audio.h"
#include "K_game.h"
#include "K_string.h"
#include "K_video.h"

/* ====
   COIN
   ==== */

static void load() {
    load_sprite_num("items/coin/%u", 3, AKL_NEVER);
    load_sound("coin", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(6);
    actor->box.start.y = Int2Fx(2);
    actor->box.end.x = Int2Fx(25);
    actor->box.end.y = Int2Fx(30);
    actor->depth = Int2Fx(2);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("items/coin/%i", (gamestate()->time / 5) % 3));
}

static void collide(GameActor* actor, GameActor* other) {
    if (other->type != ACT_PLAYER)
        return;

    GamePlayer* player = get_player(other->player);
    if (player != NULL) {
        ++player->coins;
        player->score += 200;
    }

    FLAG_ON(actor, FLG_DESTROY);
    play_state_sound("coin", PLAY_POS, A_ACTOR(actor));
}

const ActorTable TAB_COIN = {
    .load = load,
    .create = create,
    .draw = draw,
    .collide = collide,
};
