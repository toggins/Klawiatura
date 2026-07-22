#include "K_game.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

/* ====
   BUSH
   ==== */

static void load_bush() {
    load_sprite_num("scenery/bush/%u", 3, AKL_NEVER);
}

static void create_bush(GameActor* actor) {
    actor->depth = Int2Fx(25);
}

static void draw_bush(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/bush/%i", ((gamestate()->time * 7) / 50) % 3));
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
    actor->depth = Int2Fx(26);
}

static void draw_cloud(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/cloud/%i", ((gamestate()->time * 2) / 25) % 3));
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
    const Sint32 ax = Fx2Int(ipos.x) + ((Sint32)(Fx2Float(VAL(actor, X_SPEED)) * totalticks()) % 64),
                 ay = Fx2Int(ipos.y);
    batch_pos(B_XYZ(ax, ay, Fx2Float(actor->depth)));
    batch_tile(B_TILE(TRUE, FALSE));
    const Sint32 w = Fx2Int(levelinfo()->size.x) + 128;
    batch_rectangle("scenery/clouds", B_WH(w, 64.f));
    batch_tile(B_NO_TILE);
}

const ActorTable TAB_CLOUDS = {
    .load = load_clouds,
    .create = create_cloud,
    .draw = draw_clouds,
};
