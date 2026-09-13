#include "K_audio.h"
#include "K_video.h"

#include "actors/K_artillery.h"
#include "actors/K_enemies.h"

/* ===========
   BULLET BILL
   =========== */

static void load_bullet_bill() {
    load_sprite("enemies/bullet", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_bullet_bill(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-13);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Int2Fx(15);

    actor->depth = Fx1;
}

static void tick_bullet_bill(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_ARTILLERY_DEAD)) {
        move_actor(actor, Vadd(actor->pos, actor->vel));
        actor->vel.y += 26214;

        if (!in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
            FLAG_ON(actor, FLG_DESTROY);
    } else {
        if (in_any_view(actor->pos, Int2Fx(-256), VEF_ALL))
            move_actor(actor, Vadd(actor->pos, actor->vel));
        else
            FLAG_ON(actor, FLG_DESTROY);
    }
}

static void draw_bullet_bill(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/bullet", FALSE);
}

static void collide_bullet_bill(GameActor* actor, GameActor* from) {
    if (ANY_FLAG(actor, FLG_ARTILLERY_DEAD))
        return;

    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (check_stomp(actor, from, Fx0, 100, TRUE))
            kill_enemy(actor, from, FALSE);

        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        block_fireball(from);
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

    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL: {
        hit_shell(actor, from);
        break;
    }
    }
}

const ActorTable TAB_BULLET_BILL = {
    .load = load_bullet_bill,
    .create = create_bullet_bill,
    .tick = tick_bullet_bill,
    .draw = draw_bullet_bill,
    .collide = collide_bullet_bill,
};
