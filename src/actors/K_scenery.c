#include "K_game.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

enum {
    VAL_SCENERY_FRAME,
    VAL_SCENERY_ALPHA,
};

#define FLG_SCENERY_ACTIVE CUSTOM_FLAG(0)

/* ====
   BUSH
   ==== */

static void load_bush() {
    load_sprite_num("scenery/bush/%u", 3, AKL_NEVER);
}

static void create_bush(GameActor* actor) {
    actor->depth = Int2Fx(31);
}

static void draw_bush(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/bush/%i", ((gamestate()->time * 7) / 50) % 3), FALSE);
}

const ActorTable TAB_BUSH = {
    .load = load_bush,
    .create = create_bush,
    .draw = draw_bush,
};

/* =====
   CLOUD
   ===== */

static void load_cloud() {
    load_sprite_num("scenery/cloud/%u", 3, AKL_NEVER);
}

static void create_cloud(GameActor* actor) {
    actor->depth = Int2Fx(33);
}

static void draw_cloud(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/cloud/%i", ((gamestate()->time * 2) / 25) % 3), FALSE);
}

const ActorTable TAB_CLOUD = {
    .load = load_cloud,
    .create = create_cloud,
    .draw = draw_cloud,
};

/* ======
   CLOUDS
   ====== */

static void load_clouds() {
    load_sprite("scenery/clouds", AKL_NEVER);
}

static void draw_clouds(const GameActor* actor) {
    batch_reset();
    const FVec2 ipos = get_interp(actor);
    const Sint32 ax = Fx2Int(ipos.x) + ((Sint32)(Fx2Float(actor->vel.x) * screenticks()) % 64), ay = Fx2Int(ipos.y);
    batch_pos(B_F3(ax, ay, Fx2Float(actor->depth)));
    batch_tile(B_B2(TRUE, FALSE));
    const Sint32 w = Fx2Int(levelinfo()->size.x) + 128;
    batch_rectangle("scenery/clouds", B_F2(w, 64.f));
    batch_tile(B_B2_FALSE);
}

const ActorTable TAB_CLOUDS = {
    .load = load_clouds,
    .create = create_cloud,
    .draw = draw_clouds,
};

/* ==========
   LAMP LIGHT
   ========== */

static void load_lamp_light() {
    load_sprite_num("scenery/lamp/light/%u", 6, AKL_NEVER);
}

static void create_lamp_light(GameActor* actor) {
    actor->depth = Int2Fx(-33);

    VAL(actor, SCENERY_ALPHA) = 50;
}

static void tick_lamp_light(GameActor* actor) {
    VAL(actor, SCENERY_ALPHA) += rng(5);
    VAL(actor, SCENERY_ALPHA) -= rng(5);

    if (VAL(actor, SCENERY_ALPHA) > 100 || VAL(actor, SCENERY_ALPHA) < 20)
        VAL(actor, SCENERY_ALPHA) = 50;
}

static void draw_lamp_light(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA((1.f - ((float)VAL(actor, SCENERY_ALPHA) / 128.f)) * 255.f));
    draw_actor(actor, fmt("scenery/lamp/light/%i", ((gamestate()->time * 2) / 5) % 6), FALSE);
}

const ActorTable TAB_LAMP_LIGHT = {
    .load = load_lamp_light,
    .create = create_lamp_light,
    .tick = tick_lamp_light,
    .draw = draw_lamp_light,
};

/* ============
   TUBE BUBBLES
   ============ */

static void load_tube_bubbles() {
    load_actor(ACT_TUBE_BUBBLE);
}

static void tick_tube_bubbles(GameActor* actor) {
    if (!in_any_view(actor->pos, Int2Fx(-32), FALSE) || (gamestate()->time % 5) != 0 || rng(11) != 10)
        return;

    Sint32 r = rng(10);
    r -= rng(10);
    GameActor* bubble = create_actor(ACT_TUBE_BUBBLE, Vadd(actor->pos, (FVec2){Int2Fx(r), Int2Fx(-4)}));
    if (bubble != NULL)
        bubble->vel.y = -8192 - (rng(60) * 8192);
}

const ActorTable TAB_TUBE_BUBBLES = {
    .load = load_tube_bubbles,
    .tick = tick_tube_bubbles,
};
