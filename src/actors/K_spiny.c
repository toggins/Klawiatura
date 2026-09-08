#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

#define FLG_SPINY_GRAY CUSTOM_ENEMY_FLAG(0)

/* =====
   SPINY
   ===== */

static void load() {
    load_sprite_num("enemies/spiny/%u", 2, AKL_NEVER);
    load_sprite("enemies/spiny/dead", AKL_NEVER);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SPINY_GRAY)) {
        load_sprite_num("enemies/spiny/gray/%u", 2, AKL_NEVER);
        load_sprite("enemies/spiny/gray", AKL_NEVER);
    }
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-29);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;
}

static void tick(GameActor* actor) {
    VAL(actor, ENEMY_FRAME) += 7;

    move_enemy(actor,
        (FVec2){
            ((gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) && !ANY_FLAG(actor, FLG_SPINY_GRAY)) ? Int2Fx(2) : Fx1,
            19005},
        FALSE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny/gray/%i" : "enemies/spiny/%i",
            (VAL(actor, ENEMY_FRAME) / 50) % 2),
        FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny/gray/dead" : "enemies/spiny/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        maybe_hit_player(actor, from);
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
        if (ANY_FLAG(actor, FLG_SPINY_GRAY))
            block_fireball(from);
        else
            hit_fireball(actor, from, 100);

        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        if (ANY_FLAG(actor, FLG_SPINY_GRAY))
            block_beetroot(from);
        else
            hit_beetroot(actor, from, 100);

        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }
    }
}

const ActorTable TAB_SPINY = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};
