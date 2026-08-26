#include "K_video.h"

#include "actors/K_goal.h"
#include "actors/K_points.h"

/* ========
   GOAL BAR
   ======== */

static void load() {
    load_sprite("markers/goal/bar", AKL_NEVER);
    load_sprite("markers/goal/bar_fly", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-23);
    actor->box.end.x = Int2Fx(21);
    actor->box.end.y = Int2Fx(16);

    actor->depth = Int2Fx(19);

    actor->vel.y = 196608;
    VAL(actor, BAR_Y) = actor->pos.y;
}

static void tick(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_BAR_FLY)) {
        VAL(actor, BAR_ANGLE) += 25736;

        move_actor(actor, Vadd(actor->pos, actor->vel));
        actor->vel.y += 13107;

        if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32)))
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));
    if (actor->vel.y > Fx0) {
        const Fixed bottom = VAL(actor, BAR_Y) + Int2Fx(221);
        if (actor->pos.y >= bottom) {
            move_actor(actor, (FVec2){actor->pos.x, bottom});
            actor->vel.y = -actor->vel.y;
        }
    } else if (actor->vel.y < Fx0 && actor->pos.y <= VAL(actor, BAR_Y)) {
        move_actor(actor, (FVec2){actor->pos.x, VAL(actor, BAR_Y)});
        actor->vel.y = -actor->vel.y;
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    batch_angle(Fx2Float(VAL(actor, BAR_ANGLE)));
    draw_actor(actor, ANY_FLAG(actor, FLG_BAR_FLY) ? "markers/goal/bar_fly" : "markers/goal/bar", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_BAR_FLY) || get_sequence()->type == GS_WIN)
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    if (actor->pos.y < (VAL(actor, BAR_Y) + Int2Fx(30)))
        give_points(actor, player, 10000);
    else if (actor->pos.y < (VAL(actor, BAR_Y) + Int2Fx(60)))
        give_points(actor, player, 5000);
    else if (actor->pos.y < (VAL(actor, BAR_Y) + Int2Fx(100)))
        give_points(actor, player, 2000);
    else if (actor->pos.y < (VAL(actor, BAR_Y) + Int2Fx(150)))
        give_points(actor, player, 1000);
    else if (actor->pos.y < (VAL(actor, BAR_Y) + Int2Fx(200)))
        give_points(actor, player, 500);
    else
        give_points(actor, player, 200);

    if (ANY_FLAG(actor, FLG_BAR_JACKPOT)) {
        const FVec2 pos = actor->pos;
        move_actor(actor, (FVec2){actor->pos.x, VAL(actor, BAR_Y) - Int2Fx(30)});
        give_points(actor, player, 1000000);
        move_actor(actor, pos);
    }

    win_player(player);

    const Fixed dir = Fmul(Fdiv(19 + rng(3), 32), Fx2Pi);
    actor->vel.x = Fmul(327680, Fcos(dir));
    actor->vel.y = Fmul(327680, Fsin(dir));

    FLAG_ON(actor, FLG_BAR_FLY);
}

const ActorTable TAB_GOAL_BAR = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* =========
   GOAL MARK
   ========= */

static void create_mark(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void collide_mark(GameActor* actor, GameActor* from) {
    (void)actor;

    if (from->type != ACT_PLAYER || !TOUCHING(from, TOUCH_BOTTOM) || get_sequence()->type == GS_WIN)
        return;

    GamePlayer* player = get_player(from->player);
    give_points(NULL, player, 100);
    win_player(player);
}

const ActorTable TAB_GOAL_MARK = {
    .create = create_mark,
    .collide = collide_mark,
};
