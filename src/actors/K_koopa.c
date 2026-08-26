#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_koopa.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

/* =====
   KOOPA
   ===== */

static void load() {
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_KOOPA_SHELL);
    load_actor(ACT_POINTS);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_KOOPA_RED)) {
        load_sprite_num("enemies/koopa/red/%u", 2, AKL_NEVER);
        load_sprite("enemies/koopa/red/dead", AKL_NEVER);
    } else {
        load_sprite_num("enemies/koopa/%u", 2, AKL_NEVER);
        load_sprite("enemies/koopa/dead", AKL_NEVER);
    }
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-13);
    actor->box.start.y = Int2Fx(-27);
    actor->box.end.x = Int2Fx(19);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;

    increase_ambush();
}

static void cleanup(GameActor* actor) {
    (void)actor;

    decrease_ambush();
}

static void tick(GameActor* actor) {
    const Bool red = ANY_FLAG(actor, FLG_KOOPA_RED);
    VAL(actor, ENEMY_FRAME) += red ? 9 : 6;
    move_enemy(actor, (FVec2){ANY_FLAG(actor, FLG_KOOPA_RED) ? Int2Fx(2) : Fx1, 19005}, red);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/koopa/red/%i" : "enemies/koopa/%i",
            (VAL(actor, ENEMY_FRAME) / 100) % 2),
        FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/koopa/red/dead" : "enemies/koopa/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (check_stomp(actor, from, Int2Fx(-16), 100)) {
            GameActor* shell = create_actor(ACT_KOOPA_SHELL, actor->pos);
            if (shell != NULL) {
                FLAG_ON(shell, actor->flags & FLG_KOOPA_RED);
                align_interp(shell, actor);
            }

            FLAG_ON(actor, FLG_DESTROY);
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

const ActorTable TAB_KOOPA = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};

/* ===========
   KOOPA SHELL
   =========== */

static void load_shell() {
    load_sprite("enemies/koopa/shell", AKL_NEVER);
    load_sprite_num("enemies/koopa/shell/%u", 4, AKL_NEVER);
    load_sprite("enemies/koopa/shell/red", AKL_NEVER);
    load_sprite_num("enemies/koopa/shell/red/%u", 4, AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_shell(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-27);
    actor->box.end.x = Int2Fx(17);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;
}

static void tick_shell(GameActor* actor) {
    const Fixed speed = Fabs(actor->vel.x);
    if (speed == Fx0) {
        VAL(actor, SHELL_COMBO) = VAL(actor, SHELL_FRAME) = 0;
        actor->vel.y += 13763;
    } else {
        VAL(actor, SHELL_FRAME) += ANY_FLAG(actor, FLG_KOOPA_RED) ? 59 : 54;
        actor->vel.y += 19005;
    }

    displace_actor(actor, Int2Fx(10), FALSE);

    if (VAL(actor, SHELL_COOLDOWN) <= 30)
        ++VAL(actor, SHELL_COOLDOWN);

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (actor->vel.x == Fx0) {
        if (TOUCHING(actor, TOUCH_RIGHT))
            actor->vel.x = -speed;
        else if (TOUCHING(actor, TOUCH_LEFT))
            actor->vel.x = speed;
    }

    collide_actor(actor);
}

static void draw_shell(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        (actor->vel.x == Fx0)
            ? (ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/koopa/shell/red" : "enemies/koopa/shell")
            : fmt(ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/koopa/shell/red/%i" : "enemies/koopa/shell/%i",
                  (VAL(actor, SHELL_FRAME) / 100) % 4),
        FALSE);
}

static void collide_shell(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (VAL(from, PLAYER_STARMAN) > 0) {
            player_starman(from, actor);
            break;
        }

        if (actor->vel.x == Fx0) {
            if (VAL(actor, SHELL_COOLDOWN) <= 6)
                break;

            actor->player = from->player;
            actor->vel.x = (actor->pos.x > from->pos.x) ? Int2Fx(6) : Int2Fx(-6);
            actor->vel.y = Fx0;
            VAL(actor, SHELL_COOLDOWN) = 0;

            play_state_sound("kick", PLAY_POS, A_ACTOR(actor));
        } else if (from->pos.y < (actor->pos.y - Int2Fx(14))) {
            if (VAL(actor, SHELL_COOLDOWN) <= 10)
                break;

            actor->vel.x = actor->vel.y = Fx0;
            VAL(actor, SHELL_COOLDOWN) = 0;

            GamePlayer* player = get_player(from->player);
            from->vel.y = Fmul(
                (player != NULL && ANY_INPUT(player, GI_JUMP)) ? Int2Fx(-13) : Int2Fx(-8), get_player_jump(player));
            FLAG_ON(from, FLG_PLAYER_STOMP);
            give_points(actor, player, 100);

            play_state_sound("stomp", PLAY_POS, A_ACTOR(actor));
        } else if (VAL(actor, SHELL_COOLDOWN) > 30) {
            maybe_hit_player(actor, from);
        }

        break;
    }

    case ACT_BLOCK_BUMP: {
        hit_bump(actor, from, 100);
        break;
    }

    case ACT_KOOPA_SHELL: {
        hit_shell(actor, from);
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

const ActorTable TAB_KOOPA_SHELL = {
    .load = load_shell,
    .create = create_shell,
    .tick = tick_shell,
    .draw = draw_shell,
    .draw_dead = draw_dead,
    .collide = collide_shell,
};
