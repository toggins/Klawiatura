#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

enum {
    VAL_ROTODISC_LENGTH,
    VAL_ROTODISC_ANGLE,
    VAL_ROTODISC_SPEED,
    VAL_ROTODISC_OWNER,
};

#define FLG_ROTODISC_FLOWER CUSTOM_FLAG(0)
#define FLG_ROTODISC_FLOWER2 CUSTOM_FLAG(1)

/* ==============
   ROTO-DISC BALL
   ============== */

static void load_ball() {
    load_sprite("enemies/rotodisc/ball", AKL_NEVER);
    load_actor(ACT_ROTODISC);
}

static void create_ball(GameActor* actor) {
    actor->depth = 1310719;
}

static void tick_ball(GameActor* actor) {
    if (gamestate()->time > 0)
        return;

    GameActor* rotodisc = create_actor(ACT_ROTODISC, actor->pos);
    if (rotodisc == NULL)
        return;

    VAL(rotodisc, ROTODISC_OWNER) = actor->id;
    VAL(rotodisc, ROTODISC_LENGTH) = VAL(actor, ROTODISC_LENGTH);
    VAL(rotodisc, ROTODISC_ANGLE) = VAL(actor, ROTODISC_ANGLE);
    VAL(rotodisc, ROTODISC_SPEED) = VAL(actor, ROTODISC_SPEED);
    FLAG_ON(rotodisc, actor->flags & (FLG_ROTODISC_FLOWER | FLG_ROTODISC_FLOWER2));
}

static void draw_ball(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/rotodisc/ball", FALSE);
}

const ActorTable TAB_ROTODISC_BALL = {
    .load = load_ball,
    .create = create_ball,
    .tick = tick_ball,
    .draw = draw_ball,
};

/* =========
   ROTO-DISC
   ========= */

static void load() {
    load_sprite_num("enemies/rotodisc/%u", 26, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-17);
    actor->box.start.y = Int2Fx(-15);
    actor->box.end.x = actor->box.end.y = Int2Fx(17);

    VAL(actor, ROTODISC_LENGTH) = Int2Fx(150);
    VAL(actor, ROTODISC_SPEED) = 1144;
    VAL(actor, ROTODISC_OWNER) = NULL_ACTOR;
}

static void tick(GameActor* actor) {
    const GameActor* owner = get_actor(VAL(actor, ROTODISC_OWNER));
    if (owner == NULL) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    move_actor(actor, Vadd(owner->pos, (FVec2){
                                           Fmul(Fcos(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH)),
                                           Fmul(-Fsin(VAL(actor, ROTODISC_ANGLE)), VAL(actor, ROTODISC_LENGTH)),
                                       }));
    VAL(actor, ROTODISC_ANGLE) += VAL(actor, ROTODISC_SPEED);

    if (ANY_FLAG(actor, FLG_ROTODISC_FLOWER)) {
        VAL(actor, ROTODISC_LENGTH) += ANY_FLAG(actor, FLG_ROTODISC_FLOWER2) ? Int2Fx(-5) : Int2Fx(5);
        if (VAL(actor, ROTODISC_LENGTH) >= Int2Fx(200) && !ANY_FLAG(actor, FLG_ROTODISC_FLOWER2))
            FLAG_ON(actor, FLG_ROTODISC_FLOWER2);
        else if (VAL(actor, ROTODISC_LENGTH) <= Fx0 && ANY_FLAG(actor, FLG_ROTODISC_FLOWER2))
            FLAG_OFF(actor, FLG_ROTODISC_FLOWER2);
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/rotodisc/%i", gamestate()->time % 26), FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    maybe_hit_player(actor, from);
}

const ActorTable TAB_ROTODISC = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};
