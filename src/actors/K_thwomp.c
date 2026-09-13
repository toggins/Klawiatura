#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

typedef Uint8 ThwompAnimations;
enum {
    TA_IDLE,
    TA_BLINK,
    TA_LAUGH,
};

enum {
    VAL_THWOMP_Y,
    VAL_THWOMP_WAIT,
    VAL_THWOMP_OVERLAP,
    VAL_THWOMP_ANIMATION,
    VAL_THWOMP_FRAME,
};

#define FLG_THWOMP_FALL CUSTOM_FLAG(0)
#define FLG_THWOMP_FELL CUSTOM_FLAG(1)

static void load() {
    load_sprite("enemies/thwomp", AKL_NEVER);
    load_sprite_num("enemies/thwomp/%i", 6, AKL_NEVER);
    load_sprite_num("enemies/thwomp/laugh/%i", 15, AKL_NEVER);
    load_sprite("enemies/thwomp/dead", AKL_NEVER);
    load_sound("stun", AKL_NEVER);
    load_sound("vo/thwomp", AKL_NEVER);
    load_actor(ACT_EXPLODE);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-27);
    actor->box.start.y = Int2Fx(-33);
    actor->box.end.x = Int2Fx(26);
    actor->box.end.y = Int2Fx(35);

    actor->depth = 196607;

    VAL(actor, THWOMP_Y) = actor->pos.y;
}

static void tick(GameActor* actor) {
    switch (VAL(actor, THWOMP_ANIMATION)) {
    default:
        break;

    case TA_BLINK: {
        if (++VAL(actor, THWOMP_FRAME) >= 6)
            VAL(actor, THWOMP_ANIMATION) = TA_IDLE;

        break;
    }

    case TA_LAUGH: {
        VAL(actor, THWOMP_FRAME) += 4;
        if (VAL(actor, THWOMP_FRAME) >= 375)
            VAL(actor, THWOMP_ANIMATION) = TA_IDLE;

        break;
    }
    }

    VAL_TICK(actor, THWOMP_OVERLAP);

    const FVec2 ppos = nearest_player_pos(actor->pos);
    if (actor->pos.x < (ppos.x + Int2Fx(100)) && actor->pos.x > (ppos.x - Int2Fx(100))
        && !ANY_FLAG(actor, FLG_THWOMP_FELL) && in_any_view(actor->pos, Int2Fx(-128), VEF_ALL))
    {
        FLAG_ON(actor, FLG_THWOMP_FALL);
    }

    if (ANY_FLAG(actor, FLG_THWOMP_FALL)) {
        displace_actor(actor, Fx0, FALSE);
        actor->vel.y += Fx1;
    }

    if (TOUCHING(actor, TOUCH_BOTTOM)) {
        FLAG_OFF(actor, FLG_THWOMP_FALL);
        actor->vel.y = Fx0;
        FLAG_ON(actor, FLG_THWOMP_FELL);
        TOUCH_OFF(actor, TOUCH_BOTTOM);

        create_actor(ACT_EXPLODE, Vadd(actor->pos, (FVec2){Int2Fx(-17), Int2Fx(34)}));
        create_actor(ACT_EXPLODE, Vadd(actor->pos, (FVec2){Int2Fx(17), Int2Fx(34)}));
        quake_actor(actor, (FVec2){Int2Fx(10), Fx1});

        play_state_sound("stun", PLAY_POS, A_ACTOR(actor));
    }

    if (ANY_FLAG(actor, FLG_THWOMP_FELL) && VAL(actor, THWOMP_WAIT) < 100)
        ++VAL(actor, THWOMP_WAIT);

    if (VAL(actor, THWOMP_WAIT) >= 100 && ANY_FLAG(actor, FLG_THWOMP_FELL)) {
        if (actor->pos.y > VAL(actor, THWOMP_Y))
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, -Fx1}));

        if (actor->pos.y <= VAL(actor, THWOMP_Y)) {
            FLAG_OFF(actor, FLG_THWOMP_FELL);
            VAL(actor, THWOMP_WAIT) = 0;
        }
    }

    if ((gamestate()->time % 5) == 0 && rng(20) == 5 && VAL(actor, THWOMP_ANIMATION) != TA_LAUGH) {
        VAL(actor, THWOMP_ANIMATION) = TA_BLINK;
        VAL(actor, THWOMP_FRAME) = 0;
    }
}

static void draw(const GameActor* actor) {
    batch_reset();

    const char* sprite = "enemies/thwomp";
    switch (VAL(actor, THWOMP_ANIMATION)) {
    default:
        break;
    case TA_BLINK:
        sprite = fmt("enemies/thwomp/%i", VAL(actor, THWOMP_FRAME) % 6);
        break;
    case TA_LAUGH:
        sprite = fmt("enemies/thwomp/laugh/%i", (VAL(actor, THWOMP_FRAME) / 25) % 15);
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/thwomp/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (maybe_hit_player(actor, from) && VAL(actor, THWOMP_OVERLAP) <= 0) {
            VAL(actor, THWOMP_ANIMATION) = TA_LAUGH;
            VAL(actor, THWOMP_FRAME) = 0;

            play_state_sound("vo/thwomp", PLAY_POS, A_ACTOR(actor));
        }

        VAL(actor, THWOMP_OVERLAP) = 2;
        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        block_fireball(from);
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        block_beetroot(from);
        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 1000);
        break;
    }
    }
}

const ActorTable TAB_THWOMP = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};
