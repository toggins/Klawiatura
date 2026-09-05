#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_bowser.h"
#include "actors/K_player.h"
#include "actors/K_podoboo.h"

enum {
    VAL_LAVA_WAVE,
    VAL_LAVA_Y,
    VAL_LAVA_ANGLE,
    VAL_LAVA_OVERLAP,
};

#define FLG_LAVA_WAVE CUSTOM_FLAG(0)

/* ====
   LAVA
   ==== */

static void load() {
    load_sprite_num("enemies/lava/%u", 7, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.y = Int2Fx(-17);
    actor->box.end.x = Int2Fx(32);
    actor->box.end.y = Int2Fx(31);

    actor->depth = Int2Fx(20);
}

static void tick(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_LAVA_WAVE))
        return;

    VAL_TICK(actor, LAVA_OVERLAP);

    move_actor(
        actor, (FVec2){actor->pos.x, VAL(actor, LAVA_Y) + Fmul(VAL(actor, LAVA_WAVE), Fcos(VAL(actor, LAVA_ANGLE)))});
    VAL(actor, LAVA_ANGLE) += 5719;

    if (VAL(actor, LAVA_WAVE) > Fx0) {
        VAL(actor, LAVA_WAVE) -= 6554;
        if (VAL(actor, LAVA_WAVE) < Fx0)
            VAL(actor, LAVA_WAVE) = Fx0;
    }
    if (VAL(actor, LAVA_WAVE) <= Fx0)
        VAL(actor, LAVA_ANGLE) = Fx0;
}

static void draw(const GameActor* actor) {
    batch_reset();
    const char* sprite = fmt("enemies/lava/%i", ((gamestate()->time * 11) / 100) % 7);
    for (Sint32 i = 0, n = Fx2Int(actor->box.end.x - actor->box.start.x); i < n; i += 32) {
        batch_offset(B_F3_XY(-i, 0.f));
        draw_actor(actor, sprite, FALSE);
    }
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        kill_player(from);
        break;
    }

    case ACT_BOWSER_DEAD: {
        const Fixed ly = actor->pos.y + Int2Fx(10);
        if ((from->pos.y + from->box.end.y) <= ly || VAL(from, BOWSER_DEAD_LAVA) > 0)
            break;

        create_actor(ACT_LAVA_SPLASH, Vadd(from->pos, (FVec2){Fx0, Int2Fx(-9)}));

        GameActor* waver = create_actor(ACT_LAVA_WAVER, from->pos);
        if (waver != NULL)
            waver->vel.x = 122880;
        waver = create_actor(ACT_LAVA_WAVER, from->pos);
        if (waver != NULL)
            waver->vel.x = -122880;

        from->depth = actor->depth + 1;
        ++VAL(from, BOWSER_DEAD_LAVA);
        VAL(from, BOWSER_DEAD_Y) = ly;

        play_state_sound("bowser/lava", PLAY_POS, A_ACTOR(from));
        break;
    }

    case ACT_LAVA_WAVER: {
        if (VAL(actor, LAVA_OVERLAP) > 0) {
            VAL(actor, LAVA_OVERLAP) = 2;
            break;
        }

        if ((from->vel.x > Fx0 && (from->pos.x + from->box.start.x) > (actor->pos.x + actor->box.start.x))
            || (from->vel.x < Fx0 && (from->pos.x + from->box.end.x) < (actor->pos.x + actor->box.end.x)))
        {
            break;
        }

        actor->depth -= 1;
        VAL(actor, LAVA_Y) = actor->pos.y;
        VAL(actor, LAVA_WAVE) = VAL(from, LAVA_WAVE);
        VAL(actor, LAVA_OVERLAP) = 2;
        FLAG_ON(actor, FLG_LAVA_WAVE);

        VAL(from, LAVA_WAVE) -= Int2Fx(4);
        break;
    }

    case ACT_PODOBOO: {
        if (VAL(from, PODOBOO_OVERLAP) > 0) {
            VAL(from, PODOBOO_OVERLAP) = 2;
            break;
        }

        VAL(from, PODOBOO_OVERLAP) = 2;
        if (from->vel.y >= Fmul(VAL(from, PODOBOO_JUMP), 43691))
            FLAG_OFF(from, FLG_VISIBLE);

        break;
    }
    }
}

const ActorTable TAB_LAVA = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* ==========
   LAVA WAVER
   ========== */

static void create_waver(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-17);
    actor->box.end.x = Int2Fx(17);
    actor->box.end.y = Int2Fx(15);

    VAL(actor, LAVA_WAVE) = Int2Fx(16);
    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_waver(GameActor* actor) {
    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (VAL(actor, LAVA_WAVE) <= Fx0) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    collide_actor(actor);
}

const ActorTable TAB_LAVA_WAVER = {
    .create = create_waver,
    .tick = tick_waver,
};
