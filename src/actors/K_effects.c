#include "K_game.h"
#include "K_string.h"
#include "K_video.h"

enum {
    VAL_EFFECT_FRAME,

    FLG_EFFECT_END = CUSTOM_FLAG(0),
};

/* ============
   WATER SPLASH
   ============ */

static void load_water_splash() {
    load_sprite_num("effects/water_splash/%u", 15, AKL_NEVER);
}

static void create_water_splash(GameActor* actor) {
    actor->depth = -1;
}

static void tick_water_splash(GameActor* actor) {
    VAL(actor, EFFECT_FRAME) += 7;
    if (VAL(actor, EFFECT_FRAME) >= 150)
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_water_splash(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("effects/water_splash/%i", VAL(actor, EFFECT_FRAME) / 10), FALSE);
}

const ActorTable TAB_WATER_SPLASH = {
    .create = create_water_splash,
    .load = load_water_splash,
    .tick = tick_water_splash,
    .draw = draw_water_splash,
};

/* ======
   BUBBLE
   ====== */

static void load_bubble() {
    load_sprite_num("effects/bubble/%u", 5, AKL_NEVER);
    load_sprite_num("effects/bubble/pop/%u", 7, AKL_NEVER);
}

static void tick_bubble(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_EFFECT_END)) {
        if (++VAL(actor, EFFECT_FRAME) >= 7)
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    ++VAL(actor, EFFECT_FRAME);

    FVec2 pos = actor->pos;
    pos.y -= Int2Fx(rng(3));
    pos.x += Int2Fx(rng(2));
    pos.x -= Int2Fx(rng(2));
    move_actor(actor, pos);

    const GameActor* water = get_actor(gamestate()->water);
    if (water == NULL || actor->pos.y < water->pos.y) {
        VAL(actor, EFFECT_FRAME) = 0;
        FLAG_ON(actor, FLG_EFFECT_END);
        return;
    }

    if (!in_any_view(actor->pos, Int2Fx(-32), FALSE))
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_bubble(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_EFFECT_END) ? fmt("effects/bubble/pop/%i", VAL(actor, EFFECT_FRAME))
                                        : fmt("effects/bubble/%i", (VAL(actor, EFFECT_FRAME) / 2) % 5),
        FALSE);
}

const ActorTable TAB_BUBBLE = {
    .load = load_bubble,
    .tick = tick_bubble,
    .draw = draw_bubble,
};
