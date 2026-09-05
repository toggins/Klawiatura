#include "K_game.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

enum {
    VAL_SCENERY_ANIMATION,
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

/* =========
   WATERFALL
   ========= */

static void load_waterfall() {
    load_sprite_num("scenery/waterfall/%u", 4, AKL_NEVER);
}

static void create_waterfall(GameActor* actor) {
    actor->depth = Int2Fx(189);
}

static void draw_waterfall(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/waterfall/%i", ((gamestate()->time * 41) / 100) % 4), FALSE);
}

const ActorTable TAB_WATERFALL = {
    .load = load_waterfall,
    .create = create_waterfall,
    .draw = draw_waterfall,
};

/* ========
   LAVAFALL
   ======== */

static void load_lavafall() {
    load_sprite_num("scenery/lavafall/%u", 10, AKL_NEVER);
}

static void create_lavafall(GameActor* actor) {
    actor->depth = Int2Fx(40);
}

static void draw_lavafall(const GameActor* actor) {
    batch_reset();

    const Sint32 ax = Fx2Int(get_interp(actor).x), ay = (Sint32)SDL_fmodf(screenticks() * 6.f, 32.f);
    const float az = Fx2Float(actor->depth);
    const char* sprite = fmt("scenery/lavafall/%i", gamestate()->time % 10);

    for (Sint32 i = -32, n = Fx2Int(levelinfo()->size.y); i < n; i += 32) {
        batch_pos(B_F3(ax, i + ay, az));
        batch_sprite(sprite);
    }
}

const ActorTable TAB_LAVAFALL = {
    .load = load_lavafall,
    .create = create_lavafall,
    .draw = draw_lavafall,
};

/* ============
   LAVA BUBBLER
   ============ */

static void load_lava_bubbler() {
    load_actor(ACT_LAVA_BUBBLE);
}

static void tick_lava_bubbler(GameActor* actor) {
    if (((gamestate()->time * 2) % 5) > 1 || !in_any_view(actor->pos, Int2Fx(-32), FALSE))
        return;

    FVec2 bpos = actor->pos;
    bpos.x += Int2Fx(rng(24));
    bpos.x -= Int2Fx(rng(24));

    GameActor* bubble = create_actor(ACT_LAVA_BUBBLE, bpos);
    if (bubble == NULL)
        return;

    bubble->vel.x += Int2Fx(rng(3));
    bubble->vel.x -= Int2Fx(rng(3));
    bubble->vel.y = Int2Fx(-2) - Int2Fx(rng(4));
}

const ActorTable TAB_LAVA_BUBBLER = {
    .load = load_lava_bubbler,
    .tick = tick_lava_bubbler,
};

/* ==========
   CLOUD FACE
   ========== */

static void load_cloud_face() {
    load_sprite("scenery/cloud/face", AKL_NEVER);
    load_sprite_num("scenery/cloud/face/%u", 14, AKL_NEVER);
    load_sprite_num("scenery/cloud/face/alt/%u", 2, AKL_NEVER);
}

static void create_cloud_face(GameActor* actor) {
    actor->depth = Int2Fx(69);
}

static void tick_cloud_face(GameActor* actor) {
    switch (VAL(actor, SCENERY_ANIMATION)) {
    default:
        break;

    case 1: {
        if (++VAL(actor, SCENERY_FRAME) >= 14)
            VAL(actor, SCENERY_ANIMATION) = VAL(actor, SCENERY_FRAME) = 0;

        break;
    }

    case 2: {
        if (++VAL(actor, SCENERY_FRAME) >= 50)
            VAL(actor, SCENERY_ANIMATION) = VAL(actor, SCENERY_FRAME) = 0;

        break;
    }
    }

    if ((gamestate()->time % 5) == 0 && in_any_view(actor->pos, Int2Fx(-32), FALSE)
        && VAL(actor, SCENERY_ANIMATION) == 0)
    {
        switch (rng(20)) {
        default:
            break;

        case 10: {
            VAL(actor, SCENERY_ANIMATION) = 1;
            VAL(actor, SCENERY_FRAME) = 0;
            break;
        }

        case 15: {
            VAL(actor, SCENERY_ANIMATION) = 2;
            VAL(actor, SCENERY_FRAME) = 0;
            break;
        }
        }
    }
}

static void draw_cloud_face(const GameActor* actor) {
    batch_reset();

    const char* sprite = "scenery/cloud/face";
    switch (VAL(actor, SCENERY_ANIMATION)) {
    default:
        break;
    case 1:
        sprite = fmt("scenery/cloud/face/%i", VAL(actor, SCENERY_FRAME) % 14);
        break;
    case 2:
        sprite = fmt("scenery/cloud/face/alt/%i", (VAL(actor, SCENERY_FRAME) / 25) % 2);
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

const ActorTable TAB_CLOUD_FACE = {
    .load = load_cloud_face,
    .create = create_cloud_face,
    .tick = tick_cloud_face,
    .draw = draw_cloud_face,
};
