#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

/* ======
   GOOMBA
   ====== */

static void load() {
    load_sprite_num("enemies/goomba/%u", 2, AKL_NEVER);
    load_sprite("enemies/goomba/flat", AKL_NEVER);
    load_sprite("enemies/goomba/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;

    increase_ambush();
}

static void cleanup(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_ENEMY_FLAT))
        decrease_ambush();
}

static void tick(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_ENEMY_FLAT)) {
        if (++VAL(actor, ENEMY_FRAME) > 200) {
            FLAG_ON(actor, FLG_DESTROY);
        } else {
            actor->vel.y += FxHalf;
            displace_actor(actor, Fx0, FALSE);
        }

        return;
    }

    VAL(actor, ENEMY_FRAME) += 11;
    move_enemy(actor, (FVec2){Fx1, 19005}, FALSE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_ENEMY_FLAT)
            ? "enemies/goomba/flat"
            : fmt("enemies/goomba/%i", ((VAL(actor, ENEMY_FRAME) / 100) % 2) != ANY_FLAG(actor, FLG_X_FLIP)),
        FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/goomba/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (ANY_FLAG(actor, FLG_ENEMY_FLAT))
        return;

    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (check_stomp(actor, from, Int2Fx(-16), 100)) {
            actor->vel = (FVec2){Fx0};
            actor->box.start.y = Int2Fx(-15);
            VAL(actor, ENEMY_FRAME) = 0;
            FLAG_ON(actor, FLG_ENEMY_FLAT);

            decrease_ambush();
            mark_ambush_winner(from);
            break;
        }

        maybe_hit_player(actor, from);
        break;
    }

    case ACT_GOOMBA: {
        turn_enemy(actor);
        turn_enemy(from);
        break;
    }

    case ACT_BLOCK_BUMP:
        hit_bump(actor, from, 100);
        break;
    case ACT_FIREBALL_PROJECTILE:
        hit_fireball(actor, from, 100);
        break;
    case ACT_BEETROOT_PROJECTILE:
        hit_beetroot(actor, from, 100);
        break;
    }
}

const ActorTable TAB_GOOMBA = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};
