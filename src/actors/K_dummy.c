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

    move_actor(actor, (FVec2){actor->pos.x + VAL(actor, X_SPEED), actor->pos.y + VAL(actor, Y_SPEED)});
}

static void draw(const GameActor* actor) {
    batch_reset();

    const InterpActor* iactor = get_interp(actor);
    const Sint32 ax = Fx2Int(iactor->pos.x), ay = Fx2Int(iactor->pos.y), az = Fx2Int(actor->depth);

    batch_pos(B_XYZ(ax, ay, az));
    batch_color(ANY_FLAG(actor, FLG_GLOW) ? B_YELLOW : B_WHITE);
    batch_string("hud", 16.f, fmt("%i", VAL(actor, VALUE)));
}

const GameActorTable TAB_DUMMY = {
    .create = create,
    .tick = tick,
    .draw = draw,
};
