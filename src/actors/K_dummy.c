#include "K_game.h"
#include "K_string.h"
#include "K_video.h"

enum {
    VAL_VALUE = VAL_CUSTOM,
};

enum {
    FLG_GLOW = CUSTOM_FLAG(0),
};

static void create(GameActor* actor) {
    VAL(actor, X_SPEED) = Int2Fx(5);
    VAL(actor, Y_SPEED) = Int2Fx(6) + FxHalf;
}

static void tick(GameActor* actor) {
    if (((gamestate()->time + actor->depth) % 50) == 49) {
        ++VAL(actor, VALUE);
        VAL(actor, X_SPEED) = -VAL(actor, X_SPEED);
        VAL(actor, Y_SPEED) = -VAL(actor, Y_SPEED);
    }

    move_actor(actor, POS_SPEED(actor));
}

static void draw(const GameActor* actor) {
    batch_reset();
    const FVec2 ipos = get_interp(actor);
    const Sint32 ax = Fx2Int(ipos.x), ay = Fx2Int(ipos.y);
    batch_pos(B_XYZ(ax, ay, Fx2Float(actor->depth)));
    batch_color(ANY_FLAG(actor, FLG_GLOW) ? B_YELLOW : B_WHITE);
    batch_string("hud", 16.f, fmt("%i", VAL(actor, VALUE)));
}

const ActorTable TAB_DUMMY = {
    .create = create,
    .tick = tick,
    .draw = draw,
};
