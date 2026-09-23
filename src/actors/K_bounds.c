#include "K_audio.h"
#include "K_game.h"

enum {
    VAL_BOUNDS_START_X,
    VAL_BOUNDS_START_Y,
    VAL_BOUNDS_END_X,
    VAL_BOUNDS_END_Y,
    VAL_BOUNDS_TRACK,
};

#define FLG_BOUNDS_WARP CUSTOM_FLAG(0)

static void load() {
    load_sound("warp", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || VAL(actor, BOUNDS_START_X) == VAL(actor, BOUNDS_END_X)
        || VAL(actor, BOUNDS_START_Y) == VAL(actor, BOUNDS_END_Y))
    {
        return;
    }

    GamePlayer* player = get_player(from->player);
    if (player == NULL
        || (player->bounds.start.x == VAL(actor, BOUNDS_START_X) && player->bounds.start.y == VAL(actor, BOUNDS_START_Y)
            && player->bounds.end.x == VAL(actor, BOUNDS_END_X) && player->bounds.end.y == VAL(actor, BOUNDS_END_Y)))
    {
        return;
    }

    player->bounds.start.x = VAL(actor, BOUNDS_START_X);
    player->bounds.start.y = VAL(actor, BOUNDS_START_Y);
    player->bounds.end.x = VAL(actor, BOUNDS_END_X);
    player->bounds.end.y = VAL(actor, BOUNDS_END_Y);
    set_player_track(player, VAL(actor, BOUNDS_TRACK));

    if (ANY_FLAG(actor, FLG_BOUNDS_WARP)) {
        // !!! CLIENT-SIDE !!!
        if (viewplayer() == from->player)
            play_state_sound("warp", 0, NULL);
        // !!! CLIENT-SIDE !!!
    }
}

const ActorTable TAB_BOUNDS = {
    .load = load,
    .create = create,
    .collide = collide,
};
