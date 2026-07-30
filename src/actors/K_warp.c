#include "K_audio.h"

#include "actors/K_player.h"
#include "actors/K_warp.h"

static void load() {
    load_sound("warp", AKL_NEVER);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_WARP_CALAMITY))
        load_sound("vo/clone/dead", AKL_NEVER);
    if (ANY_FLAG(actor, FLG_WARP_DEVASTATOR))
        load_sound("vo/thwomp", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-8);
    actor->box.start.y = Int2Fx(-13);
    actor->box.end.x = Int2Fx(7);
    actor->box.end.y = Fx1;

    VAL(actor, WARP_X) = actor->pos.x;
    VAL(actor, WARP_Y) = actor->pos.y;
    VAL(actor, WARP_ANGLE) = 3;
    VAL(actor, WARP_OUT_ANGLE) = 1;
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || get_actor(VAL(from, PLAYER_WARP)) != NULL || ANY_FLAG(from, FLG_PLAYER_WARP_OUT))
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    switch (VAL(actor, WARP_ANGLE)) {
    default: {
        if (VAL(from, X_TOUCH) != TOUCH_RIGHT || !ANY_INPUT(player, GI_RIGHT))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){actor->box.end.x - from->box.end.x, Fx0}));
        break;
    }

    case 1: {
        if (VAL(from, Y_TOUCH) != TOUCH_TOP || !ANY_INPUT(player, GI_UP))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){Fx0, actor->box.start.y - from->box.start.y}));
        break;
    }

    case 2: {
        if (VAL(from, X_TOUCH) != TOUCH_LEFT || !ANY_INPUT(player, GI_LEFT))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){actor->box.start.x - from->box.start.x, Fx0}));
        break;
    }

    case 3: {
        if (VAL(from, PLAYER_GROUND) <= 0 || !ANY_INPUT(player, GI_DOWN))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){Fx0, actor->box.end.y - from->box.end.y}));
        break;
    }
    }

    VAL(from, PLAYER_WARP) = actor->id;
    VAL(from, PLAYER_WARP_STATE) = 0;

    if (ANY_FLAG(actor, FLG_WARP_CALAMITY))
        play_state_sound("vo/clone/dead", 0, NULL);
    if (ANY_FLAG(actor, FLG_WARP_DEVASTATOR)) {
        play_state_sound("vo/thwomp", 0, NULL);
        stop_state_track(MAX_STATE_TRACKS);
    } else {
        play_state_sound("warp", PLAY_POS, A_ACTOR(from));
    }
}

const ActorTable TAB_WARP = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .collide = collide,
};
