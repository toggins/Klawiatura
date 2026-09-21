#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_koopa.h"

#define FLG_SPINY_GRAY CUSTOM_ENEMY_FLAG(0)
#define FLG_SPINY_HATCH CUSTOM_ENEMY_FLAG(1)
#define FLG_SPINY_OVERLAP CUSTOM_ENEMY_FLAG(2)

/* =====
   SPINY
   ===== */

static void load() {
    load_sprite_num("enemies/spiny/%u", 2, AKL_NEVER);
    load_sprite("enemies/spiny/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SPINY_GRAY)) {
        load_sprite_num("enemies/spiny/gray/%u", 2, AKL_NEVER);
        load_sprite("enemies/spiny/gray/dead", AKL_NEVER);
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
    if (ANY_FLAG(actor, FLG_SPINY_HATCH)) {
        if (++VAL(actor, ENEMY_FRAME) >= ((gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) ? 1 : 6)) {
            FLAG_OFF(actor, FLG_SPINY_HATCH);
            VAL(actor, ENEMY_FRAME) = 0;
        }
    } else {
        VAL(actor, ENEMY_FRAME) += 7;
    }

    move_enemy(actor,
        (FVec2){
            ((gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) && !ANY_FLAG(actor, FLG_SPINY_GRAY)) ? Int2Fx(2) : Fx1,
            19005},
        FALSE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_SPINY_HATCH)
            ? ((gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP))
                      ? "enemies/spiny/0"
                      : fmt("enemies/spiny/hatch/%i", VAL(actor, ENEMY_FRAME) % 6))
            : fmt(ANY_FLAG(actor, FLG_SPINY_GRAY) ? "enemies/spiny/gray/%i" : "enemies/spiny/%i",
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

    case ACT_BULLET_PROJECTILE: {
        if (ANY_FLAG(actor, FLG_SPINY_GRAY))
            block_bullet(from);
        else
            hit_bullet(actor, from, 100);

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

/* =========
   SPINY EGG
   ========= */

static void load_egg() {
    if (gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) {
        load_sprite("enemies/spiny/egg2", AKL_NEVER);
    } else {
        load_sprite("enemies/spiny/egg", AKL_NEVER);
        load_sprite_num("enemies/spiny/hatch/%u", 6, AKL_NEVER);
    }

    load_sprite("enemies/spiny/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_SPINY);
    load_actor(ACT_POINTS);
}

static void create_egg(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.end.x = Int2Fx(16);
    if (gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) {
        actor->box.start.y = Int2Fx(-18);
        actor->box.end.y = Int2Fx(20);
    } else {
        actor->box.start.y = Int2Fx(-30);
        actor->box.end.y = Fx1;
    }

    actor->depth = 65535;
}

static void tick_egg(GameActor* actor) {
    ++VAL(actor, ENEMY_FRAME);

    if (touching_solid(Radd(actor->box, actor->pos), SOL_SOLID))
        FLAG_ON(actor, FLG_SPINY_OVERLAP);
    else
        FLAG_OFF(actor, FLG_SPINY_OVERLAP);

    if (ANY_FLAG(actor, FLG_SPINY_OVERLAP))
        move_actor(actor, Vadd(actor->pos, actor->vel));
    else
        displace_actor(actor, Int2Fx(10), FALSE);

    if (below_nearest_view(actor->pos, Int2Fx(50))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (actor->vel.y < Int2Fx(8))
        actor->vel.y = Fmin(actor->vel.y + 8738, Int2Fx(8));

    if (TOUCHING(actor, TOUCH_BOTTOM)) {
        GameActor* spiny = create_actor(ACT_SPINY, actor->pos);
        if (spiny != NULL) {
            spiny->vel.x = ((gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) && !ANY_FLAG(actor, FLG_SPINY_GRAY))
                               ? Int2Fx(2)
                               : Fx1;
            if (spiny->pos.x > nearest_player_pos(spiny->pos).x) {
                spiny->vel.x = -spiny->vel.x;
                FLAG_ON(spiny, FLG_X_FLIP);
            }

            FLAG_ON(spiny, FLG_ENEMY_ACTIVE | FLG_SPINY_HATCH);

            align_interp(spiny, actor);
        }

        FLAG_ON(actor, FLG_DESTROY);
    }
}

static void draw_egg(const GameActor* actor) {
    batch_reset();
    if (gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP)) {
        batch_angle(((float)VAL(actor, ENEMY_FRAME) / 32.f) * 2.f * SDL_PI_F);
        draw_actor(actor, "enemies/spiny/egg2", FALSE);
    } else {
        batch_offset(B_F3_XY(0.f, 15.f));
        batch_angle(((float)VAL(actor, ENEMY_FRAME) / 16.f) * 2.f * SDL_PI_F);
        draw_actor(actor, "enemies/spiny/egg", FALSE);
    }
}

static void collide_egg(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        maybe_hit_player(actor, from);
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

    case ACT_BULLET_PROJECTILE: {
        if (ANY_FLAG(actor, FLG_SPINY_GRAY))
            block_bullet(from);
        else
            hit_bullet(actor, from, 100);

        break;
    }
    }
}

const ActorTable TAB_SPINY_EGG = {
    .load = load_egg,
    .create = create_egg,
    .tick = tick_egg,
    .draw = draw_egg,
    .draw_dead = draw_dead,
    .collide = collide_egg,
};
