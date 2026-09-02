#include "K_game.h"

static void load() {
    load_actor(ACT_SINK_BUBBLE);
}

static void create(GameActor* actor) {
    actor->vel.x = Int2Fx(2);
    actor->vel.y = Fx1;
}

static void tick(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (in_any_view(actor->pos, Int2Fx(-128), FALSE) && (game_state->time % 15) == 0) {
        FVec2 bpos = actor->pos;
        bpos.y -= Int2Fx(200) + Int2Fx(rng(32));
        bpos.x += Int2Fx(rng(64));
        bpos.x -= Int2Fx(rng(64));
        GameActor* bubble = create_actor(ACT_SINK_BUBBLE, bpos);
        if (bubble != NULL)
            bubble->vel.y = Fdouble(actor->vel.y);
    }

    const GameActor* water = get_actor(game_state->water);
    if (water == NULL)
        return;

    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER || pawn->pos.y <= water->pos.y
            || actor->pos.x <= (pawn->pos.x - Int2Fx(128)) || actor->pos.x >= (pawn->pos.x + Int2Fx(128)))
        {
            continue;
        }

        if (actor->pos.x < (pawn->pos.x - actor->vel.x)) {
            const FVec2 ppos = Vadd(pawn->pos, (FVec2){-actor->vel.x, Fx0});
            if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
                move_actor(pawn, ppos);
        }
        if (actor->pos.x > (pawn->pos.x + actor->vel.x)) {
            const FVec2 ppos = Vadd(pawn->pos, (FVec2){actor->vel.x, Fx0});
            if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
                move_actor(pawn, ppos);
        }

        const FVec2 ppos = Vadd(pawn->pos, (FVec2){Fx0, actor->vel.y});
        if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
            move_actor(pawn, ppos);
    }
}

const ActorTable TAB_SINK = {
    .load = load,
    .create = create,
    .tick = tick,
};
