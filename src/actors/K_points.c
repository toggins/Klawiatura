#include "K_audio.h"
#include "K_video.h"

#include "actors/K_points.h"

void give_points(GameActor* actor, GamePlayer* player, Sint32 points) {
    if (player == NULL)
        return;

    if (actor == NULL)
        actor = get_actor(player->actor);

    if (points > 0) {
        player->score += points;
    } else if (points < 0) {
        if (player->lives < 100)
            ++player->lives;
    } else {
        return;
    }

    if (actor == NULL)
        return;

    GameActor* pobj = create_actor(ACT_POINTS, Vadd(actor->pos, (FVec2){Fx0, actor->box.start.y}));
    if (pobj == NULL)
        return;

    pobj->player = player->id;
    VAL(pobj, POINTS) = points;
    if (points < 0)
        play_state_sound("1up", PLAY_POS, A_ACTOR(pobj));
}

/* ======
   POINTS
   ====== */

static void load() {
    load_sprite("effects/points/100", AKL_NEVER);
    load_sprite("effects/points/200", AKL_NEVER);
    load_sprite("effects/points/500", AKL_NEVER);
    load_sprite("effects/points/1000", AKL_NEVER);
    load_sprite("effects/points/2000", AKL_NEVER);
    load_sprite("effects/points/5000", AKL_NEVER);
    load_sprite("effects/points/10000", AKL_NEVER);
    load_sprite("effects/points/1up", AKL_NEVER);
    load_sprite("effects/points/1000000", AKL_NEVER);
    load_sound("1up", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->depth = Int2Fx(-1000);
}

static void tick(GameActor* actor) {
    ++VAL(actor, POINTS_TIME);
    if (VAL(actor, POINTS_TIME) < 35)
        move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, -73728}));

    if ((VAL(actor, POINTS) < 0 && VAL(actor, POINTS_TIME) > 100)
        || (VAL(actor, POINTS) >= 100 && VAL(actor, POINTS) < 200 && VAL(actor, POINTS_TIME) > 80)
        || (VAL(actor, POINTS) >= 200 && VAL(actor, POINTS) < 1000 && VAL(actor, POINTS_TIME) > 100)
        || (VAL(actor, POINTS) >= 1000 && VAL(actor, POINTS) < 2000 && VAL(actor, POINTS_TIME) > 150)
        || (VAL(actor, POINTS) >= 2000 && VAL(actor, POINTS) < 5000 && VAL(actor, POINTS_TIME) > 200)
        || (VAL(actor, POINTS) >= 5000 && VAL(actor, POINTS) < 10000 && VAL(actor, POINTS_TIME) > 220)
        || (VAL(actor, POINTS) >= 10000 && VAL(actor, POINTS) < 1000000 && VAL(actor, POINTS_TIME) > 300)
        || (VAL(actor, POINTS) >= 1000000 && VAL(actor, POINTS_TIME) > 1500))
    {
        FLAG_ON(actor, FLG_DESTROY);
    }
}

static void draw(const GameActor* actor) {
    if (actor->player != viewplayer())
        return;

    batch_reset();
    draw_actor(actor,
        (VAL(actor, POINTS) < 0)
            ? "effects/points/1up"
            : ((VAL(actor, POINTS) >= 100 && VAL(actor, POINTS) < 200)            ? "effects/points/100"
                  : (VAL(actor, POINTS) >= 200 && VAL(actor, POINTS) < 500)       ? "effects/points/200"
                  : (VAL(actor, POINTS) >= 500 && VAL(actor, POINTS) < 1000)      ? "effects/points/500"
                  : (VAL(actor, POINTS) >= 1000 && VAL(actor, POINTS) < 2000)     ? "effects/points/1000"
                  : (VAL(actor, POINTS) >= 2000 && VAL(actor, POINTS) < 5000)     ? "effects/points/2000"
                  : (VAL(actor, POINTS) >= 5000 && VAL(actor, POINTS) < 10000)    ? "effects/points/5000"
                  : (VAL(actor, POINTS) >= 10000 && VAL(actor, POINTS) < 1000000) ? "effects/points/10000"
                                                                                  : "effects/points/1000000"),
        FALSE);
}

const ActorTable TAB_POINTS = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
};
