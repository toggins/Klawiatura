#include "actors/K_screen.h"

/* ==========
   AUTOSCROLL
   ========== */

static void cleanup_autoscroll(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->autoscroll == actor->id)
        game_state->autoscroll = NULL_ACTOR;
}

static void pre_tick_autoscroll(GameActor* actor) {
    GameState* game_state = gamestate();

    if (game_state->time == 0 && !ANY_FLAG(actor, FLG_SCROLL_TANKS))
        FLAG_OFF(actor, FLG_VISIBLE);

    if (game_state->autoscroll == actor->id) {
        const LevelInfo* level_info = levelinfo();
        if (actor->pos.x < level_info->bounds.start.x || actor->pos.x > level_info->bounds.end.x
            || actor->pos.y < level_info->bounds.start.y || actor->pos.y > level_info->bounds.end.y)
        {
            return;
        }

        FVec2 npos = Vadd(actor->pos, actor->vel);
        if (!ANY_FLAG(actor, FLG_SCROLL_BOWSER | FLG_SCROLL_TANKS) && get_sequence()->type == GS_WIN) {
            if (actor->vel.x != Fx0)
                npos.x += (actor->vel.x > Fx0) ? Int2Fx(4) : Int2Fx(-4);
            if (actor->vel.y != Fx0)
                npos.y += (actor->vel.y > Fx0) ? Int2Fx(4) : Int2Fx(-4);
        }

        move_actor(actor, npos);
        return;
    } else if (!in_any_view(actor->pos, Fx0, FALSE)) {
        return;
    }

    GameActor* autoscroll = get_actor(game_state->autoscroll);
    if (autoscroll == NULL) {
        game_state->autoscroll = actor->id;
        autoscroll = actor;
    } else {
        autoscroll->vel = actor->vel;
        VAL(autoscroll, SCROLL_TRACK) = VAL(actor, SCROLL_TRACK);
        autoscroll->flags = actor->flags;
        FLAG_ON(actor, FLG_DESTROY);
    }

    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        GamePlayer* player = get_player(i);
        set_player_track(player, VAL(autoscroll, SCROLL_TRACK));
        if (!in_player_view(player, player->pos, Fx0, FALSE))
            respawn_player(player);
    }
}

const ActorTable TAB_AUTOSCROLL = {
    .pre_tick = pre_tick_autoscroll,
};
