#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_koopa.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

static void collide_generic_shell(GameActor* actor, GameActor* from) {
    if (actor == NULL || from == NULL || from->type != ACT_PLAYER)
        return;

    if (VAL(from, PLAYER_STARMAN) > 0) {
        player_starman(from, actor);
        return;
    }

    if (actor->vel.x == Fx0) {
        if (VAL(actor, SHELL_COOLDOWN) <= 6)
            return;

        actor->player = from->player;
        actor->vel.x = (actor->pos.x > from->pos.x) ? Int2Fx(6) : Int2Fx(-6);
        actor->vel.y = Fx0;
        VAL(actor, SHELL_COOLDOWN) = 0;

        play_state_sound("kick", PLAY_POS, A_ACTOR(actor));
    } else if (from->pos.y < (actor->pos.y - Int2Fx(14))) {
        if (VAL(actor, SHELL_COOLDOWN) <= 10)
            return;

        actor->vel.x = actor->vel.y = Fx0;
        VAL(actor, SHELL_COOLDOWN) = 0;

        GamePlayer* player = get_player(from->player);
        from->vel.y
            = Fmul((player != NULL && ANY_INPUT(player, GI_JUMP)) ? Int2Fx(-13) : Int2Fx(-8), get_player_jump(player));
        FLAG_ON(from, FLG_PLAYER_STOMP);
        give_points(actor, player, 100);

        play_state_sound("stomp", PLAY_POS, A_ACTOR(actor));
    } else if (VAL(actor, SHELL_COOLDOWN) > 30) {
        maybe_hit_player(actor, from);
    }
}

/* =====
   KOOPA
   ===== */

static void load() {
    load_sprite_num("enemies/koopa/%u", 2, AKL_NEVER);
    load_sprite_num("enemies/koopa/red/%u", 2, AKL_NEVER);
    load_sprite("enemies/koopa/dead", AKL_NEVER);
    load_sprite("enemies/koopa/red/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_KOOPA_SHELL);
    load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-27);
    actor->box.end.x = Int2Fx(16);
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

    move_enemy(actor, (FVec2){red ? Int2Fx(2) : Fx1, 19005}, red);

    if (VAL(actor, KOOPA_MAYDAY) < 11)
        ++VAL(actor, KOOPA_MAYDAY);
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
        if (VAL(actor, KOOPA_MAYDAY) <= 10 && VAL(from, PLAYER_STARMAN) <= 0)
            break;

        if (!check_stomp(actor, from, Int2Fx(-16), 100, TRUE))
            break;

        GameActor* shell = create_actor(ACT_KOOPA_SHELL, actor->pos);
        if (shell != NULL) {
            FLAG_ON(shell, actor->flags & FLG_KOOPA_RED);
            align_interp(shell, actor);
        }

        FLAG_ON(actor, FLG_DESTROY);
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
    }
}

const ActorTable TAB_KOOPA = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
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
    load_sprite("enemies/koopa/dead", AKL_NEVER);
    load_sprite("enemies/koopa/shell/red", AKL_NEVER);
    load_sprite_num("enemies/koopa/shell/red/%u", 4, AKL_NEVER);
    load_sprite("enemies/koopa/red/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_shell(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-27);
    actor->box.end.x = Int2Fx(16);
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
    case ACT_PLAYER:
        collide_generic_shell(actor, from);
        break;
    case ACT_BLOCK_BUMP:
        hit_bump(actor, from, 100);
        break;
    case ACT_KOOPA_SHELL:
        hit_shell(actor, from);
        break;
    case ACT_FIREBALL_PROJECTILE:
        hit_fireball(actor, from, 100);
        break;
    case ACT_BEETROOT_PROJECTILE:
        hit_beetroot(actor, from, 100);
        break;
    case ACT_HAMMER_PROJECTILE:
        hit_hammer(actor, from, 100);
        break;
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

/* ==========
   PARATROOPA
   ========== */

static void load_paratroopa() {
    load_sprite_num("enemies/koopa/paratroopa/%u", 2, AKL_NEVER);
    load_sprite("enemies/koopa/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_KOOPA);
    load_actor(ACT_POINTS);
}

static void load_paratroopa_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_KOOPA_RED)) {
        load_sprite_num("enemies/koopa/paratroopa/red/%u", 2, AKL_NEVER);
        load_sprite("enemies/koopa/red/dead", AKL_NEVER);
    }
}

static void create_paratroopa(GameActor* actor) {
    create(actor);

    VAL(actor, KOOPA_X) = actor->pos.x;
    VAL(actor, KOOPA_Y) = actor->pos.y;
    VAL(actor, KOOPA_ANGLE) = rng(360) * 1144;
}

static void tick_paratroopa(GameActor* actor) {
    VAL(actor, ENEMY_FRAME) += 3;

    move_actor(actor,
        (FVec2){
            VAL(actor, KOOPA_X)
                - (ANY_FLAG(actor, FLG_KOOPA_HORIZONTAL) ? Fmul(Int2Fx(50), Fsin(VAL(actor, KOOPA_ANGLE))) : Fx0),
            VAL(actor, KOOPA_Y)
                + (ANY_FLAG(actor, FLG_KOOPA_HORIZONTAL) ? Fx0 : Fmul(Int2Fx(50), Fcos(VAL(actor, KOOPA_ANGLE)))),
        });
    VAL(actor, KOOPA_ANGLE) = Fmod(VAL(actor, KOOPA_ANGLE) + 2288, Fx2Pi);

    const FVec2 ppos = nearest_player_pos(actor->pos);
    if (actor->pos.x > ppos.x)
        FLAG_ON(actor, FLG_X_FLIP);
    else if (actor->pos.x < ppos.x)
        FLAG_OFF(actor, FLG_X_FLIP);
}

static void draw_paratroopa(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_KOOPA_RED) ? "enemies/koopa/paratroopa/red/%i" : "enemies/koopa/paratroopa/%i",
            (VAL(actor, ENEMY_FRAME) / 25) % 2),
        FALSE);
}

static void collide_paratroopa(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (!check_stomp(actor, from, Int2Fx(-16), 100, TRUE))
            break;

        GameActor* koopa = create_actor(ACT_KOOPA, actor->pos);
        if (koopa != NULL) {
            FLAG_ON(koopa, actor->flags & (FLG_X_FLIP | FLG_KOOPA_RED));
            align_interp(koopa, actor);
        }

        FLAG_ON(actor, FLG_DESTROY);
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

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }
    }
}

const ActorTable TAB_PARATROOPA = {
    .load = load_paratroopa,
    .load_special = load_paratroopa_special,
    .create = create_paratroopa,
    .tick = tick_paratroopa,
    .draw = draw_paratroopa,
    .draw_dead = draw_dead,
    .collide = collide_paratroopa,
};

/* =====
   BUZZY
   ===== */

static void load_buzzy() {
    load_sprite_num("enemies/buzzy/%u", 2, AKL_NEVER);
    load_sprite("enemies/buzzy/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_BUZZY_SHELL);
    load_actor(ACT_POINTS);
}

static void create_buzzy(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(15);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;

    increase_ambush();
}

static void tick_buzzy(GameActor* actor) {
    VAL(actor, ENEMY_FRAME) += 13;
    move_enemy(actor, (FVec2){Fx1, 19005}, FALSE);
}

static void draw_buzzy(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/buzzy/%i", (VAL(actor, ENEMY_FRAME) / 100) % 2), FALSE);
}

static void draw_dead_buzzy(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/buzzy/dead", FALSE);
}

static void collide_buzzy(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (!check_stomp(actor, from, Int2Fx(-16), 100, TRUE))
            break;

        GameActor* shell = create_actor(ACT_BUZZY_SHELL, actor->pos);
        if (shell != NULL)
            align_interp(shell, actor);

        FLAG_ON(actor, FLG_DESTROY);
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
        block_fireball(from);
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        block_beetroot(from);
        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }
    }
}

const ActorTable TAB_BUZZY = {
    .load = load_buzzy,
    .create = create_buzzy,
    .cleanup = cleanup,
    .tick = tick_buzzy,
    .draw = draw_buzzy,
    .draw_dead = draw_dead_buzzy,
    .collide = collide_buzzy,
};

/* ===========
   BUZZY SHELL
   =========== */

static void load_buzzy_shell() {
    load_sprite("enemies/buzzy/shell", AKL_NEVER);
    load_sprite_num("enemies/buzzy/shell/%u", 4, AKL_NEVER);
    load_sprite("enemies/buzzy/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_buzzy_shell(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-29);
    actor->box.end.x = Int2Fx(15);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;
}

static void draw_buzzy_shell(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        (actor->vel.x == Fx0) ? "enemies/buzzy/shell"
                              : fmt("enemies/buzzy/shell/%i", (VAL(actor, SHELL_FRAME) / 100) % 4),
        FALSE);
}

static void collide_buzzy_shell(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;
    case ACT_PLAYER:
        collide_generic_shell(actor, from);
        break;
    case ACT_BLOCK_BUMP:
        hit_bump(actor, from, 100);
        break;
    case ACT_KOOPA_SHELL:
        hit_shell(actor, from);
        break;
    case ACT_FIREBALL_PROJECTILE:
        block_fireball(from);
        break;
    case ACT_BEETROOT_PROJECTILE:
        block_beetroot(from);
        break;
    case ACT_HAMMER_PROJECTILE:
        hit_hammer(actor, from, 100);
        break;
    }
}

const ActorTable TAB_BUZZY_SHELL = {
    .load = load_buzzy_shell,
    .create = create_buzzy_shell,
    .tick = tick_shell,
    .draw = draw_buzzy_shell,
    .draw_dead = draw_dead_buzzy,
    .collide = collide_buzzy_shell,
};
