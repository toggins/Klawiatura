#include "K_game.h"
#include "K_string.h"
#include "K_video.h"

enum {
    VAL_VALUE = VAL_CUSTOM,
};

enum {
    FLG_GLOW = CUSTOM_FLAG(0),
};

static void tick(GameActor* actor) {
    if (((gamestate()->time + actor->pos.x + actor->pos.y + actor->depth) % 50) == 0)
        ++VAL(actor, VALUE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    const Sint32 ax = Fx2Int(actor->pos.x), ay = Fx2Int(actor->pos.y), az = Fx2Int(actor->depth);
    batch_pos(B_XYZ(ax, ay, az));
    batch_color(ANY_FLAG(actor, FLG_GLOW) ? B_YELLOW : B_WHITE);
    batch_string("hud", 16.f, fmt("%i", VAL(actor, VALUE)));
}

const GameActorTable TAB_DUMMY = {
    .tick = tick,
    .draw = draw,
};
