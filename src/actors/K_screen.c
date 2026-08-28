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
        FVec2 npos = Vadd(actor->pos, actor->vel);
        if (!ANY_FLAG(actor, FLG_SCROLL_BOWSER | FLG_SCROLL_TANKS) && get_sequence()->type == GS_WIN) {
            if (actor->vel.x != Fx0)
                npos.x += (actor->vel.x > Fx0) ? Int2Fx(4) : Int2Fx(-4);
            if (actor->vel.y != Fx0)
                npos.y += (actor->vel.y > Fx0) ? Int2Fx(4) : Int2Fx(-4);
        }

        const LevelInfo* level_info = levelinfo();
        move_actor(actor, Vclamp(npos, level_info->bounds.start, Vsub(level_info->bounds.end, F_SCREEN)));
        return;
    }

    const GamePlayer* player = NULL;
    const PlayerID n = gamecontext()->num_players;
    for (PlayerID i = 0; i < n; i++) {
        player = get_player(i);
        if (player == NULL)
            continue;

        if (ANY_FLAG(actor, FLG_SCROLL_BOWSER)) {
            if (player->pos.x > actor->pos.x)
                break;
        } else if (in_player_view(player, actor->pos, Fx0, FALSE)) {
            break;
        }

        player = NULL;
    }

    if (player == NULL)
        return;

    GameActor* autoscroll = get_actor(game_state->autoscroll);
    if (autoscroll == NULL) {
        game_state->autoscroll = actor->id;
        autoscroll = actor;

        if (ANY_FLAG(actor, FLG_SCROLL_BOWSER)) {
            move_actor(actor, Vsub(player->pos, F_HALF_SCREEN));
            skip_interp(actor);
        }
    } else {
        autoscroll->vel = actor->vel;
        VAL(autoscroll, SCROLL_TRACK) = VAL(actor, SCROLL_TRACK);
        autoscroll->flags = actor->flags;
        FLAG_ON(actor, FLG_DESTROY);
    }

    for (PlayerID i = 0; i < n; i++) {
        GamePlayer* oplayer = get_player(i);
        if (oplayer == NULL)
            continue;

        set_player_track(oplayer, VAL(autoscroll, SCROLL_TRACK));
        if (oplayer->id != player->id && !in_player_view(oplayer, oplayer->pos, Int2Fx(-32), TRUE))
            respawn_player(oplayer);
    }
}

const ActorTable TAB_AUTOSCROLL = {
    .pre_tick = pre_tick_autoscroll,
};
