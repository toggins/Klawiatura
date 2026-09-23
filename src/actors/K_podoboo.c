#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_podoboo.h"

static void load() {
    load_sprite_num("enemies/podoboo/%u", 3, AKL_NEVER);
    load_actor(ACT_LAVA_SPLASH);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-13);
    actor->box.start.y = Int2Fx(-15);
    actor->box.end.x = Int2Fx(14);
    actor->box.end.y = Int2Fx(17);

    VAL(actor, PODOBOO_JUMP) = Int2Fx(12);
    VAL(actor, PODOBOO_TIME) = 250;
    VAL(actor, PODOBOO_Y) = actor->pos.y;
    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_VISIBLE)) {
        if ((VAL(actor, PODOBOO_TIME) > 1 && (gamestate()->time % VAL(actor, PODOBOO_TIME)) > 0)
            || !in_any_view(Vsub(actor->pos, (FVec2){Int2Fx(16), Int2Fx(-16)}), Int2Fx(-640), VEF_ALL))
        {
            return;
        }

        actor->vel.y = -VAL(actor, PODOBOO_JUMP);
        VAL(actor, PODOBOO_FRAME) = 0;
        FLAG_ON(actor, FLG_VISIBLE);
    } else {
        VAL(actor, PODOBOO_FRAME) += ANY_FLAG(actor, FLG_Y_FLIP) ? 65 : 60;
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 13107;

    if (below_nearest_bounds(actor->pos, Int2Fx(32)))
        FLAG_ON(actor, FLG_DESTROY);

    if (actor->vel.y > Fx0)
        FLAG_ON(actor, FLG_Y_FLIP);
    else if (actor->vel.y < Fx0)
        FLAG_OFF(actor, FLG_Y_FLIP);

    VAL_TICK(actor, PODOBOO_OVERLAP);
    collide_actor(actor);
    if (!ANY_FLAG(actor, FLG_VISIBLE)
        || (!ANY_FLAG(actor, FLG_PODOBOO_VOLCANO) && actor->pos.y > VAL(actor, PODOBOO_Y)
            && actor->vel.y >= Fmul(VAL(actor, PODOBOO_JUMP), 43691)))
    {
        create_actor(ACT_LAVA_SPLASH, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(15)}));
        if (ANY_FLAG(actor, FLG_PODOBOO_VOLCANO)) {
            FLAG_ON(actor, FLG_DESTROY);
        } else {
            move_actor(actor, (FVec2){actor->pos.x, VAL(actor, PODOBOO_Y)});
            FLAG_OFF(actor, FLG_VISIBLE);
        }
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/podoboo/%i", (VAL(actor, PODOBOO_FRAME) / 100) % 3), FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        maybe_hit_player(actor, from);
        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 1000);
        break;
    }
    }
}

const ActorTable TAB_PODOBOO = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* ===============
   PODOBOO VOLCANO
   =============== */

static void load_volcano() {
    load_sound("fire", AKL_NEVER);
    load_actor(ACT_PODOBOO);
}

static void create_volcano(GameActor* actor) {
    VAL(actor, VOLCANO_TIME) = 300;
    VAL(actor, VOLCANO_FIRE_TIME) = 15;
    VAL(actor, VOLCANO_FIRE_AMOUNT) = 8;
}

static void tick_volcano(GameActor* actor) {
    if (--VAL(actor, VOLCANO_STATE) <= 0) {
        if (VAL(actor, VOLCANO_FIRE) <= 0)
            VAL(actor, VOLCANO_FIRE) = VAL(actor, VOLCANO_FIRE_AMOUNT);

        VAL(actor, VOLCANO_STATE) += VAL(actor, VOLCANO_TIME);
    }

    if (--VAL(actor, VOLCANO_FIRE_STATE) <= 0) {
        if (VAL(actor, VOLCANO_FIRE) > 0 && in_any_view(actor->pos, Int2Fx(-256), VEF_ALL)) {
            GameActor* podoboo = create_actor(ACT_PODOBOO, actor->pos);
            if (podoboo != NULL) {
                podoboo->vel.x = Int2Fx(rng(5));
                podoboo->vel.x -= Int2Fx(rng(5));
                podoboo->vel.y = Int2Fx(-11);
                FLAG_ON(podoboo, FLG_VISIBLE | FLG_PODOBOO_VOLCANO);
            }

            --VAL(actor, VOLCANO_FIRE);

            play_state_sound("fire", PLAY_POS, A_ACTOR(actor));
        }

        VAL(actor, VOLCANO_FIRE_STATE) += VAL(actor, VOLCANO_FIRE_TIME);
    }
}

const ActorTable TAB_VOLCANO = {
    .load = load_volcano,
    .create = create_volcano,
    .tick = tick_volcano,
};
