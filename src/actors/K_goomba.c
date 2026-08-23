#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

enum {
    VAL_PARTY_TYPE,
};

static void load() {
    load_sprite_num("enemies/goomba/%u", 2, AKL_NEVER);
    load_sprite("enemies/goomba/flat", AKL_NEVER);
    load_sprite("enemies/goomba/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
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
            ++actor->depth;
            actor->vel.x = actor->vel.y = Fx0;
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

    case ACT_GOOMBA:
    case ACT_KOOPA: {
        turn_enemy(actor);
        turn_enemy(from);
        break;
    }

    case ACT_KOOPA_SHELL: {
        if (!hit_shell(actor, from))
            turn_enemy(actor);
        break;
    }

    case ACT_BLOCK_BUMP: {
        hit_bump(actor, from, 100);
        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        hit_fireball(actor, from, 100);
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        hit_beetroot(actor, from, 100);
        break;
    }
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

/* ============
   GOOMBA PARTY
   ============ */

static void load_party_special(const GameActor* actor) {
    load_actor(VAL(actor, PARTY_TYPE));
}

static void create_party(GameActor* actor) {
    VAL(actor, PARTY_TYPE) = ACT_GOOMBA;
}

static void tick_party(GameActor* actor) {
    if ((gamestate()->time % 50) != 0)
        return;

    Bool found = FALSE;
    FVec2 ppos = {Fx0};
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER || (found && pawn->pos.x <= ppos.x))
            continue;

        ppos = pawn->pos;
        found = TRUE;
    }

    GameActor* goomba = create_actor(VAL(actor, PARTY_TYPE), Vadd(ppos, (FVec2){Int2Fx(201 + rng(200)), Int2Fx(-498)}));
    if (goomba != NULL)
        FLAG_ON(goomba, FLG_X_FLIP);
}

const ActorTable TAB_GOOMBA_PARTY = {
    .load_special = load_party_special,
    .create = create_party,
    .tick = tick_party,
};
