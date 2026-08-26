#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_points.h"

enum {
    VAL_COIN_POP_Y,
    VAL_COIN_POP_FRAME,
};

#define FLG_COIN_POP_START CUSTOM_FLAG(0)

static void give_coin(GamePlayer* player) {
    if (player == NULL)
        return;

    ++player->coins;
    player->score += 200;

    while (player->coins >= 100) {
        give_points(NULL, player, -1);
        player->coins -= 100;
    }
}

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
    draw_actor(actor, fmt("items/coin/%i", (gamestate()->time / 5) % 3), FALSE);
}

static void collide(GameActor* actor, GameActor* other) {
    switch (other->type) {
    default:
        break;

    case ACT_PLAYER: {
        give_coin(get_player(other->player));
        FLAG_ON(actor, FLG_DESTROY);

        play_state_sound("coin", PLAY_POS, A_ACTOR(actor));

        break;
    }

    case ACT_BLOCK_BUMP: {
        GameActor* pop = create_actor(ACT_COIN_POP, Vadd(actor->pos, (FVec2){Int2Fx(15), Int2Fx(29)}));
        if (pop == NULL)
            break;

        pop->player = other->player;
        FLAG_ON(actor, FLG_DESTROY);

        break;
    }
    }
}

const ActorTable TAB_COIN = {
    .load = load,
    .create = create,
    .draw = draw,
    .collide = collide,
};

/* ===========
   POPPED COIN
   =========== */

static void load_pop() {
    load_sprite_num("effects/coin_pop/%u", 21, AKL_NEVER);
    load_sound("coin", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_pop(GameActor* actor) {
    VAL(actor, COIN_POP_Y) = actor->pos.y;

    play_state_sound("coin", PLAY_POS, A_ACTOR(actor));
}

static void tick_pop(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_COIN_POP_START)) {
        give_coin(get_player(actor->player));
        FLAG_ON(actor, FLG_COIN_POP_START);
    }

    if (actor->pos.y > (VAL(actor, COIN_POP_Y) - Int2Fx(30)))
        actor->vel.y = -278528;
    else if (actor->pos.y > (VAL(actor, COIN_POP_Y) - Int2Fx(41)))
        actor->vel.y = -212992;
    else if (actor->pos.y > (VAL(actor, COIN_POP_Y) - Int2Fx(52)))
        actor->vel.y = -106496;
    else
        actor->vel.y = Fx0;

    move_actor(actor, Vadd(actor->pos, actor->vel));

    VAL(actor, COIN_POP_FRAME) += 70;
    if (VAL(actor, COIN_POP_FRAME) >= 2100) {
        GameActor* points = create_actor(ACT_POINTS, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(22)}));
        if (points != NULL) {
            points->player = actor->player;
            VAL(points, POINTS) = 200;
        }

        FLAG_ON(actor, FLG_DESTROY);
    }
}

static void draw_pop(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("effects/coin_pop/%i", VAL(actor, COIN_POP_FRAME) / 100), AKL_NEVER);
}

const ActorTable TAB_COIN_POP = {
    .load = load_pop,
    .create = create_pop,
    .tick = tick_pop,
    .draw = draw_pop,
};
