#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

typedef Uint8 BooAnimations;
enum {
    BOOA_IDLE,
    BOOA_BLINK,
    BOOA_ACTIVE,
};

enum {
    VAL_BOO_ANIMATION,
    VAL_BOO_FRAME,

    VAL_EERIE_FRAME = 0,
};

/* ===
   BOO
   === */

static void load_boo() {
    load_sprite_num("enemies/boo/%u", 2, AKL_NEVER);
    load_sprite_num("enemies/boo/blink/%u", 10, AKL_NEVER);
    load_sprite("enemies/boo/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_boo(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(2);
}

static void tick_boo(GameActor* actor) {
    if (VAL(actor, BOO_ANIMATION) == BOOA_BLINK) {
        if (++VAL(actor, BOO_FRAME) >= 10) {
            VAL(actor, BOO_ANIMATION) = BOOA_IDLE;
            VAL(actor, BOO_FRAME) = 0;
        }
    }

    Bool active = FALSE;

    Fixed angle = Fx0, score = FxUpper;
    const GameActor* nearest = NULL;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (!in_player_view(player, actor->pos, Int2Fx(-64), VEF_ALL))
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER)
            continue;

        const FVec2 ppos = Vadd(pawn->pos, (FVec2){Fx1, Int2Fx(-15)});
        const Fixed dist = Vdist(actor->pos, ppos);
        if (nearest == NULL || dist < score) {
            nearest = pawn;
            score = dist;
            angle = Fmul(Ffloor(Fdiv(Vtheta(actor->pos, ppos), 12868) + FxHalf), 12868);
        }
    }

    if (nearest != NULL) {
        if (actor->pos.x > nearest->pos.x)
            FLAG_ON(actor, FLG_X_FLIP);
        else if (actor->pos.x < nearest->pos.x)
            FLAG_OFF(actor, FLG_X_FLIP);

        if ((ANY_FLAG(actor, FLG_X_FLIP) && ANY_FLAG(nearest, FLG_X_FLIP))
            || (!ANY_FLAG(actor, FLG_X_FLIP) && !ANY_FLAG(nearest, FLG_X_FLIP)))
        {
            active = TRUE;
        }
    }

    if (active) {
        move_actor(actor, Vadd(actor->pos, (FVec2){Fmul(73728, Fcos(angle)), Fmul(73728, -Fsin(angle))}));

        VAL(actor, BOO_ANIMATION) = BOOA_ACTIVE;
        VAL(actor, BOO_FRAME) = 0;
    } else {
        if (VAL(actor, BOO_ANIMATION) == BOOA_ACTIVE) {
            VAL(actor, BOO_ANIMATION) = BOOA_IDLE;
            VAL(actor, BOO_FRAME) = 0;
        }

        if (VAL(actor, BOO_ANIMATION) == BOOA_IDLE && (gamestate()->time % 5) == 0 && rng(20) == 10) {
            VAL(actor, BOO_ANIMATION) = BOOA_BLINK;
            VAL(actor, BOO_FRAME) = 0;
        }
    }
}

static void draw_boo(const GameActor* actor) {
    batch_reset();

    const char* sprite = "enemies/boo/0";
    switch (VAL(actor, BOO_ANIMATION)) {
    default:
        break;
    case BOOA_BLINK:
        sprite = fmt("enemies/boo/blink/%i", VAL(actor, BOO_FRAME) % 10);
        break;
    case BOOA_ACTIVE:
        sprite = "enemies/boo/1";
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_dead_boo(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/boo/dead", FALSE);
}

static void collide_boo(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;
    case ACT_PLAYER:
        maybe_hit_player(actor, from);
        break;
    case ACT_HAMMER_PROJECTILE:
        hit_hammer(actor, from, 200);
        break;
    }
}

const ActorTable TAB_BOO = {
    .load = load_boo,
    .create = create_boo,
    .tick = tick_boo,
    .draw = draw_boo,
    .draw_dead = draw_dead_boo,
    .collide = collide_boo,
};

/* =====
   EERIE
   ===== */

static void load_eerie() {
    load_sprite_num("enemies/eerie/%u", 2, AKL_NEVER);
    load_sprite("enemies/eerie/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void tick_eerie(GameActor* actor) {
    VAL(actor, EERIE_FRAME) += 3;

    if (in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        move_actor(actor, Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-3) : Int2Fx(3), Fx0}));
}

static void draw_eerie(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/eerie/%i", (VAL(actor, EERIE_FRAME) / 25) % 2), FALSE);
}

static void draw_dead_eerie(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/eerie/dead", FALSE);
}

const ActorTable TAB_EERIE = {
    .load = load_eerie,
    .create = create_boo,
    .tick = tick_eerie,
    .draw = draw_eerie,
    .draw_dead = draw_dead_eerie,
    .collide = collide_boo,
};
