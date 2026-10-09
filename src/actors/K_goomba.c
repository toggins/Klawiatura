#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_koopa.h"

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
        if (!check_stomp(actor, from, Int2Fx(-16), 100, TRUE))
            break;

        ++actor->depth;
        actor->vel.x = actor->vel.y = Fx0;
        actor->box.start.y = Int2Fx(-15);
        VAL(actor, ENEMY_FRAME) = 0;
        FLAG_ON(actor, FLG_ENEMY_FLAT);

        decrease_ambush();
        mark_ambush_winner(from);
        break;
    }

    case ACT_GOOMBA:
    case ACT_KOOPA:
    case ACT_SPINY:
    case ACT_CLONE:
    case ACT_CODER_CLONE:
    case ACT_CLONE_3A:
    case ACT_BUZZY:
    case ACT_SHY_GUY: {
        turn_enemy(actor);
        turn_enemy(from);
        break;
    }

    case ACT_PARATROOPA: {
        if (ANY_FLAG(from, FLG_KOOPA_BOUNCE)) {
            turn_enemy(actor);
            turn_enemy(from);
        }

        break;
    }

    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL: {
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

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }

    case ACT_BULLET_PROJECTILE: {
        hit_bullet(actor, from, 100);
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
    if ((gamestate()->time % 50) > 0)
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

    GameActor* goomba
        = create_actor(VAL(actor, PARTY_TYPE), Vadd(ppos, (FVec2){Int2Fx(201) + Int2Fx(rng(200)), Int2Fx(-498)}));
    if (goomba != NULL)
        FLAG_ON(goomba, FLG_X_FLIP);
}

const ActorTable TAB_GOOMBA_PARTY = {
    .load_special = load_party_special,
    .create = create_party,
    .tick = tick_party,
};

/* =====
   CLONE
   ===== */

static void load_clone() {
    load_sprite_num("enemies/clone/a/%u", 15, AKL_NEVER);
    load_sprite_num("enemies/clone/a/flat/%u", 5, AKL_NEVER);
    load_sprite("enemies/clone/a/dead", AKL_NEVER);
    load_sound("vo/clone/a_stomp", AKL_NEVER);
    load_sound("vo/clone/a_dead", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_clone(GameActor* actor) {
    actor->box.start.x = Int2Fx(-19);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(18);
    actor->box.end.y = Int2Fx(23);

    actor->depth = Fx1;

    increase_ambush();
}

static void tick_clone(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_ENEMY_FLAT)) {
        if (++VAL(actor, ENEMY_FRAME) > 200) {
            FLAG_ON(actor, FLG_DESTROY);
        } else {
            actor->vel.y += FxHalf;
            displace_actor(actor, Fx0, FALSE);
        }

        return;
    }

    ++VAL(actor, ENEMY_FRAME);
    move_enemy(actor, (FVec2){Fx1, 19005}, FALSE);
}

static void draw_clone(const GameActor* actor) {
    batch_reset();

    const char* sprite = NULL;
    if (ANY_FLAG(actor, FLG_ENEMY_FLAT)) {
        const ActorValue frame = VAL(actor, ENEMY_FRAME) * 70;
        sprite = (frame >= 500) ? "enemies/clone/a/flat/4" : fmt("enemies/clone/a/flat/%i", frame / 100);
    } else {
        sprite = fmt("enemies/clone/a/%i", VAL(actor, ENEMY_FRAME) % 15);
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_dead_clone(const GameActor* actor) {
    batch_reset();
    batch_angle((float)VAL(actor, DEAD_FRAME) * 0.125f * SDL_PI_F);
    draw_actor(actor, "enemies/clone/a/dead", FALSE);
}

static void collide_clone(GameActor* actor, GameActor* from) {
    if (ANY_FLAG(actor, FLG_ENEMY_FLAT))
        return;

    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (!check_stomp(actor, from, Int2Fx(-16), 100, TRUE))
            break;

        ++actor->depth;
        actor->vel.x = actor->vel.y = Fx0;
        actor->box.start.y = Int2Fx(-32);
        actor->box.end.y = Int2Fx(20);

        VAL(actor, ENEMY_FRAME) = 0;
        FLAG_ON(actor, FLG_ENEMY_FLAT);
        FLAG_OFF(actor, FLG_X_FLIP);

        decrease_ambush();
        mark_ambush_winner(from);
        break;
    }

    case ACT_GOOMBA:
    case ACT_KOOPA:
    case ACT_SPINY:
    case ACT_CLONE:
    case ACT_CODER_CLONE:
    case ACT_CLONE_3A:
    case ACT_BUZZY:
    case ACT_SHY_GUY: {
        turn_enemy(actor);
        turn_enemy(from);
        break;
    }

    case ACT_PARATROOPA: {
        if (ANY_FLAG(from, FLG_KOOPA_BOUNCE)) {
            turn_enemy(actor);
            turn_enemy(from);
        }

        break;
    }

    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL: {
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

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }

    case ACT_BULLET_PROJECTILE: {
        hit_bullet(actor, from, 100);
        break;
    }
    }
}

const ActorTable TAB_CLONE = {
    .load = load_clone,
    .create = create_clone,
    .tick = tick_clone,
    .draw = draw_clone,
    .draw_dead = draw_dead_clone,
    .collide = collide_clone,
};
