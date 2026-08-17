#include "K_string.h"
#include "K_video.h"

#include "actors/K_effects.h"

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

/* ===========
   BRICK SHARD
   =========== */

static void load_brick_shard() {
    load_sprite_num("effects/brick_shard/%u", 4, AKL_NEVER);
    load_sprite_num("effects/brick_shard/gray/%u", 4, AKL_NEVER);
}

static void tick_brick_shard(GameActor* actor) {
    VAL(actor, EFFECT_FRAME) += 7;

    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 26214;

    if (below_nearest_bounds(actor->pos, Int2Fx(32)))
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_brick_shard(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_EFFECT_ALT) ? "effects/brick_shard/gray/%i" : "effects/brick_shard/%i",
            (VAL(actor, EFFECT_FRAME) / 25) % 4),
        FALSE);
}

const ActorTable TAB_BRICK_SHARD = {
    .load = load_brick_shard,
    .tick = tick_brick_shard,
    .draw = draw_brick_shard,
};

/* =========
   EXPLOSION
   ========= */

static void load_explode() {
    load_sprite_num("effects/explode/%u", 3, AKL_NEVER);
}

static void tick_explode(GameActor* actor) {
    VAL(actor, EFFECT_FRAME) += 24;
    if (VAL(actor, EFFECT_FRAME) >= 300)
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_explode(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("effects/explode/%i", VAL(actor, EFFECT_FRAME) / 100), FALSE);
}

const ActorTable TAB_EXPLODE = {
    .load = load_explode,
    .tick = tick_explode,
    .draw = draw_explode,
};
