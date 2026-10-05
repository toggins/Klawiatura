#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_artillery.h"
#include "actors/K_enemies.h"
#include "actors/K_screen.h"

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

    case ACT_BULLET_PROJECTILE: {
        block_bullet(from);
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

/* ============
   BILL BLASTER
   ============ */

static void load_bill_blaster() {
    load_sound("bang/0", AKL_NEVER);
    load_actor(ACT_EXPLODE);
    load_actor(ACT_BULLET_BILL);
}

static void create_bill_blaster(GameActor* actor) {
    VAL(actor, ARTILLERY_FIRE_SPEED) = 1;
    VAL(actor, ARTILLERY_BULLET_SPEED) = 212992;

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_bill_blaster(GameActor* actor) {
    const GameState* game_state = gamestate();
    if ((game_state->time % 10) == 0)
        FLAG_OFF(actor, FLG_ARTILLERY_BLOCKED);

    if (any_in_range((FVec2){actor->pos.x, Int2Fx(80)}))
        FLAG_ON(actor, FLG_ARTILLERY_BLOCKED);

    if (!ANY_FLAG(actor, FLG_ARTILLERY_BLOCKED) && in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        VAL(actor, ARTILLERY_FIRE) += VAL(actor, ARTILLERY_FIRE_SPEED);

    if (VAL(actor, ARTILLERY_FIRE) > 25) {
        const FVec2 ppos = nearest_player_pos(actor->pos);
        if (actor->pos.x > ppos.x || actor->pos.x < ppos.x) {
            VAL(actor, ARTILLERY_FIRE) = -50 - rng(150);

            GameActor* bullet = create_actor(ACT_BULLET_BILL, actor->pos);
            if (bullet != NULL) {
                if (actor->pos.x > ppos.x) {
                    bullet->vel.x = -VAL(actor, ARTILLERY_BULLET_SPEED);
                    FLAG_ON(bullet, FLG_X_FLIP);
                } else {
                    bullet->vel.x = VAL(actor, ARTILLERY_BULLET_SPEED);
                }

                bullet->vel.x = Fmul(bullet->vel.x, (game_state->flags & GF_FUNNY_TANKS) ? Int2Fx(2) : Fx1);
            }

            create_actor(
                ACT_EXPLODE, Vadd(actor->pos, (FVec2){(actor->pos.x > ppos.x) ? Int2Fx(-16) : Int2Fx(16), Fx0}));

            play_state_sound("bang/0", PLAY_POS, A_ACTOR(actor));
        }
    }
}

const ActorTable TAB_BILL_BLASTER = {
    .load = load_bill_blaster,
    .create = create_bill_blaster,
    .tick = tick_bill_blaster,
};

/* ==========
   WHEEL LEFT
   ========== */

static void load_wheel_left() {
    load_sprite_num("tiles/wheel/0/%u", 3, AKL_NEVER);
}

static void create_wheel(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);
    actor->depth = Int2Fx(20);
}

static void tick_wheel(GameActor* actor) {
    const GameActor* autoscroll = get_actor(gamestate()->autoscroll);
    if (autoscroll != NULL && ANY_FLAG(autoscroll, FLG_SCROLL_TANKS) && autoscroll->pos.x != autoscroll->last_pos.x)
        ++VAL(actor, ARTILLERY_FRAME);
}

static void draw_wheel_left(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("tiles/wheel/0/%i", (VAL(actor, ARTILLERY_FRAME) / 2) % 3), FALSE);
}

const ActorTable TAB_WHEEL_L = {
    .is_solid = always_solid,
    .load = load_wheel_left,
    .create = create_wheel,
    .tick = tick_wheel,
    .draw = draw_wheel_left,
};

/* ============
   WHEEL MIDDLE
   ============ */

static void load_wheel() {
    load_sprite_num("tiles/wheel/1/%u", 4, AKL_NEVER);
}

static void draw_wheel(const GameActor* actor) {
    batch_reset();
    for (Sint32 i = 0, n = Fx2Int(actor->box.end.x); i < n; i += 32) {
        batch_offset(B_F3_XY(-i, 0.f));
        draw_actor(actor, fmt("tiles/wheel/1/%i", (VAL(actor, ARTILLERY_FRAME) / 2) % 4), FALSE);
    }
}

const ActorTable TAB_WHEEL = {
    .is_solid = always_solid,
    .load = load_wheel,
    .create = create_wheel,
    .tick = tick_wheel,
    .draw = draw_wheel,
};

/* ===========
   WHEEL RIGHT
   =========== */

static void load_wheel_right() {
    load_sprite_num("tiles/wheel/2/%u", 3, AKL_NEVER);
}

static void draw_wheel_right(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("tiles/wheel/2/%i", (VAL(actor, ARTILLERY_FRAME) / 2) % 3), FALSE);
}

const ActorTable TAB_WHEEL_R = {
    .is_solid = always_solid,
    .load = load_wheel_right,
    .create = create_wheel,
    .tick = tick_wheel,
    .draw = draw_wheel_right,
};

/* =================
   SPIKE BALL CANNON
   ================= */

static void load_spike_ball_cannon() {
    load_sprite("enemies/spike_ball_cannon", AKL_NEVER);
    load_sound("bang/1", AKL_NEVER);
    load_actor(ACT_SPIKE_BALL_PROJECTILE);
    load_actor(ACT_EXPLODE);
}

static void create_spike_ball_cannon(GameActor* actor) {
    actor->box.start.y = Int2Fx(22);
    actor->box.end.x = actor->box.end.y = Int2Fx(64);

    actor->depth = 1310719;
}

static void tick_spike_ball_cannon(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (in_any_view(actor->pos, Int2Fx(-64), VEF_ALL)
        && ((game_state->time % 50) == 0 || (game_state->flags & GF_FUNNY_TANKS)))
    {
        ++VAL(actor, ARTILLERY_FIRE);
    }

    if (VAL(actor, ARTILLERY_FIRE) > ((game_state->flags & GF_FUNNY_TANKS) ? 40 : 2)) {
        const FVec2 spos = Vadd(actor->pos, (FVec2){Int2Fx(31), Int2Fx(22)});

        GameActor* spike = create_actor(ACT_SPIKE_BALL_PROJECTILE, spos);
        if (spike != NULL) {
            spike->vel.x = Int2Fx(rng(3));
            spike->vel.x -= Int2Fx(rng(3));
            spike->vel.y = Int2Fx(-7) - Int2Fx(rng(5));
        }

        create_actor(ACT_EXPLODE, spos);
        VAL(actor, ARTILLERY_FIRE) = 0;

        play_state_sound("bang/1", PLAY_POS, A_FVEC2(spos));
    }
}

static void draw_spike_ball_cannon(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/spike_ball_cannon", FALSE);
}

const ActorTable TAB_SPIKE_BALL_CANNON = {
    .is_solid = always_solid,
    .load = load_spike_ball_cannon,
    .create = create_spike_ball_cannon,
    .tick = tick_spike_ball_cannon,
    .draw = draw_spike_ball_cannon,
};
