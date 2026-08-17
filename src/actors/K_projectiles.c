#include "K_audio.h"
#include "K_video.h"

#include "actors/K_effects.h"
#include "actors/K_projectiles.h"

/* ========
   FIREBALL
   ======== */

static void load_fireball() {
    load_sprite("projectiles/fireball", AKL_NEVER);
    load_actor(ACT_EXPLODE);
}

static void create_fireball(GameActor* actor) {
    actor->box.start.x = actor->box.start.y = Int2Fx(-7);
    actor->box.end.x = Int2Fx(8);
    actor->box.end.y = Int2Fx(9);
}

static void tick_fireball(GameActor* actor) {
    VAL(actor, PROJECTILE_ANGLE) += 12868;

    displace_actor(actor, Int2Fx(10), FALSE);
    collide_actor(actor);

    actor->vel.y += 26214;

    GamePlayer* player = get_player(actor->player);
    if ((player == NULL && !in_any_view(actor->pos, Int2Fx(-8), TRUE))
        || (player != NULL && !in_player_view(player, actor->pos, Int2Fx(-8), TRUE)))
    {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (TOUCHING(actor, TOUCH_LEFT | TOUCH_RIGHT))
        FLAG_ON(actor, FLG_PROJECTILE_HIT);
    if (TOUCHING(actor, TOUCH_BOTTOM))
        actor->vel.y = Int2Fx(-5);

    if (!ANY_FLAG(actor, FLG_PROJECTILE_HIT))
        return;

    align_interp(create_actor(ACT_EXPLODE, actor->pos), actor);
    FLAG_ON(actor, FLG_DESTROY);
}

static void draw_fireball(const GameActor* actor) {
    batch_reset();
    batch_angle(Fx2Float(VAL(actor, PROJECTILE_ANGLE)));
    draw_actor(actor, "projectiles/fireball", FALSE);
}

const ActorTable TAB_FIREBALL_PROJECTILE = {
    .load = load_fireball,
    .create = create_fireball,
    .tick = tick_fireball,
    .draw = draw_fireball,
};

/* ========
   BEETROOT
   ======== */

static void load_beetroot() {
    load_sprite("projectiles/beetroot", AKL_NEVER);
    load_sound("stun", AKL_NEVER);
    load_actor(ACT_EXPLODE);
    load_actor(ACT_BUBBLE);
}

static void create_beetroot(GameActor* actor) {
    actor->box.start.x = Int2Fx(-11);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(12);
    actor->box.end.y = Fx1;

    VAL(actor, PROJECTILE_HITS) = 3;
}

static void tick_beetroot(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_PROJECTILE_SINK)) {
        move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(rng(3))}));

        const GameState* game_state = gamestate();
        if ((game_state->time % 10) == 0) {
            ActorID num_bubbles = 0;

            const GameActor* bubble = NULL;
            FOR_EACH_ACTOR (bubble) {
                if (bubble->type == ACT_BUBBLE && !ANY_FLAG(bubble, FLG_EFFECT_END)) {
                    if (++num_bubbles >= 20)
                        break;
                }
            }

            if (num_bubbles < 20)
                create_actor(ACT_BUBBLE, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(-3)}));
        }

        const GameActor* water = get_actor(game_state->water);
        if (water == NULL || actor->pos.y <= water->pos.y)
            FLAG_OFF(actor, FLG_PROJECTILE_SINK);

        return;
    }

    const Bool dead = VAL(actor, PROJECTILE_HITS) <= 0;
    if (dead) {
        move_actor(actor, Vadd(actor->pos, actor->vel));
    } else {
        if (ANY_FLAG(actor, FLG_PROJECTILE_OVERLAP)) {
            if (!touching_solid(Radd(actor->box, actor->pos), SOL_SOLID))
                FLAG_OFF(actor, FLG_PROJECTILE_OVERLAP);

            move_actor(actor, Vadd(actor->pos, actor->vel));
            TOUCH_OFF(actor, TOUCH_SIDES);
        } else {
            displace_actor_soft(actor);
        }

        collide_actor(actor);
    }

    actor->vel.y += 26214;

    if (below_nearest_bounds(actor->pos, Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    const GamePlayer* player = get_player(actor->player);
    if ((player == NULL && in_any_x_view(actor->pos.x, Fx0))
        || (player != NULL && !in_player_x_view(player, actor->pos.x, Fx0)))
    {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (!dead && TOUCHING(actor, TOUCH_SIDES))
        FLAG_ON(actor, FLG_PROJECTILE_HIT | FLG_PROJECTILE_OVERLAP);

    const GameActor* water = get_actor(gamestate()->water);
    if (water != NULL && (actor->pos.y - Fmax(actor->vel.y, Fx0)) > water->pos.y) {
        actor->vel.x = actor->vel.y = Fx0;
        FLAG_ON(actor, FLG_PROJECTILE_SINK | FLG_PROJECTILE_OVERLAP);
    }

    if (!ANY_FLAG(actor, FLG_PROJECTILE_HIT))
        return;

    if (!ANY_FLAG(actor, FLG_PROJECTILE_HIT_ENEMY)) {
        create_actor(ACT_EXPLODE, Vadd(actor->pos, (FVec2){Fx0, Fx1}));
        if (!ANY_FLAG(actor, FLG_PROJECTILE_HIT_BLOCK))
            play_state_sound("stun", PLAY_POS, A_ACTOR(actor));
    }

    actor->vel.x = -actor->vel.x;
    actor->vel.y = Int2Fx(-8);
    VAL_TICK(actor, PROJECTILE_HITS);
    FLAG_OFF(actor, FLG_PROJECTILE_HIT | FLG_PROJECTILE_HIT_ENEMY | FLG_PROJECTILE_HIT_BLOCK);
}

static void draw_beetroot(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "projectiles/beetroot", FALSE);
}

const ActorTable TAB_BEETROOT_PROJECTILE = {
    .load = load_beetroot,
    .create = create_beetroot,
    .tick = tick_beetroot,
    .draw = draw_beetroot,
};
