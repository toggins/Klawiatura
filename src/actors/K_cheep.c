#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

enum {
    VAL_CHEEP_SPEED,
    VAL_CHEEP_ANGLE,
    VAL_CHEEP_FRAME,
};

#define FLG_CHEEP_ACTIVE CUSTOM_FLAG(1)
#define FLG_CHEEP_TOUCHED_WATER CUSTOM_FLAG(1)
#define FLG_CHEEP_OVERLAP CUSTOM_FLAG(2)

/* ===================
   CHEEP CHEEP SPAWNER
   =================== */

static void load_spawner() {
    load_actor(ACT_CHEEP);
}

static void create_spawner(GameActor* actor) {
    actor->vel.x = -73728;
}

static void tick_spawner(GameActor* actor) {
    const GameState* game_state = gamestate();
    const GameActor* water = get_actor(game_state->water);
    if (water == NULL || below_nearest_bounds(water->pos, Fx0) || (game_state->time % 100) != 0)
        return;

    const PlayerID n = gamecontext()->num_players;
    if (get_num_actors(ACT_CHEEP) >= (10 * n))
        return;

    Fixed edge = Fx0;
    if (actor->vel.x < Fx0) {
        edge = FxLower;
        for (PlayerID i = 0; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL) {
                edge = Fmax(edge, Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
                                      player->bounds.end.x - F_HALF_SCREEN_WIDTH));
            }
        }
    } else if (actor->vel.x > Fx0) {
        edge = FxUpper;
        for (PlayerID i = 0; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL) {
                edge = Fmin(edge, Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
                                      player->bounds.end.x - F_HALF_SCREEN_WIDTH));
            }
        }
    }

    FVec2 cpos = Vsub(actor->pos, water->pos);
    cpos.x += edge - F_HALF_SCREEN_WIDTH;
    cpos.y += water->pos.y + Int2Fx(rng(300));
    GameActor* cheep = create_actor(ACT_CHEEP, cpos);
    if (cheep == NULL)
        return;

    cheep->vel.x = actor->vel.x;
    if (actor->vel.x > Fx0) {
        VAL(cheep, CHEEP_SPEED) = actor->vel.x;
    } else if (actor->vel.x < Fx0) {
        VAL(cheep, CHEEP_SPEED) = -actor->vel.x;
        VAL(cheep, CHEEP_ANGLE) = FxPi;
        FLAG_ON(cheep, FLG_X_FLIP);
    }
}

const ActorTable TAB_CHEEP_SPAWNER = {
    .load = load_spawner,
    .tick = tick_spawner,
};

/* ===========
   CHEEP CHEEP
   =========== */

static void load() {
    load_sprite_num("enemies/cheep/%i", 24, AKL_NEVER);
    load_sprite_num("enemies/cheep/alt/%i", 2, AKL_NEVER);
    load_sprite("enemies/cheep/dead", AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;
}

static void tick(GameActor* actor) {
    ++VAL(actor, CHEEP_FRAME);

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    const GameState* game_state = gamestate();
    if ((game_state->time % 50) == 0) {
        if (Fabs(VAL(actor, CHEEP_ANGLE)) > FxPi2) {
            VAL(actor, CHEEP_ANGLE) = 193019 + (rng(3) * 12868);
            FLAG_ON(actor, FLG_X_FLIP);
        } else {
            VAL(actor, CHEEP_ANGLE) = 12868 - (rng(3) * 12868);
            FLAG_OFF(actor, FLG_X_FLIP);
        }

        actor->vel.x = Fmul(VAL(actor, CHEEP_SPEED), Fcos(VAL(actor, CHEEP_ANGLE)));
        actor->vel.y = Fmul(VAL(actor, CHEEP_SPEED), -Fsin(VAL(actor, CHEEP_ANGLE)));
    }

    if (actor->vel.x > Fx0) {
        Fixed edge = FxLower;
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL) {
                edge = Fmax(edge, Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
                                      player->bounds.end.x - F_HALF_SCREEN_WIDTH)
                                      + F_HALF_SCREEN_WIDTH + Int2Fx(100));
            }
        }
        if (actor->pos.x > edge) {
            FLAG_ON(actor, FLG_DESTROY);
            return;
        }
    } else if (actor->vel.x < Fx0) {
        Fixed edge = FxUpper;
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL) {
                edge = Fmin(edge, Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
                                      player->bounds.end.x - F_HALF_SCREEN_WIDTH)
                                      - F_HALF_SCREEN_WIDTH - Int2Fx(100));
            }
        }
        if (actor->pos.x < edge) {
            FLAG_ON(actor, FLG_DESTROY);
            return;
        }
    }

    const GameActor* water = get_actor(game_state->water);
    if (ANY_FLAG(actor, FLG_CHEEP_TOUCHED_WATER)) {
        if (water == NULL || (actor->pos.y + actor->box.end.y) <= water->pos.y
            || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
        {
            FLAG_OFF(actor, FLG_CHEEP_TOUCHED_WATER);
        }
    } else if (water != NULL && (actor->pos.y + actor->box.end.y) > water->pos.y
               && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16)))
    {
        VAL(actor, CHEEP_ANGLE) = 218755;
        actor->vel.x = Fmul(VAL(actor, CHEEP_SPEED), Fcos(VAL(actor, CHEEP_ANGLE)));
        actor->vel.y = Fmul(VAL(actor, CHEEP_SPEED), -Fsin(VAL(actor, CHEEP_ANGLE)));

        FLAG_ON(actor, FLG_CHEEP_TOUCHED_WATER);
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/cheep/%i", (VAL(actor, CHEEP_FRAME) / 2) % 24), FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/cheep/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        maybe_hit_player(actor, from);
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

    case ACT_KOOPA_SHELL: {
        hit_shell(actor, from);
        break;
    }
    }
}

const ActorTable TAB_CHEEP = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};

/* ================
   BLUE CHEEP CHEEP
   ================ */

static void load_blue() {
    load_sprite_num("enemies/cheep/blue/%u", 2, AKL_NEVER);
    load_sound("bump", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_blue(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;
}

static void tick_blue(GameActor* actor) {
    VAL(actor, CHEEP_FRAME) += 9;

    if (ANY_FLAG(actor, FLG_CHEEP_ACTIVE))
        move_actor(actor, Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? -81920 : 81920, Fx0}));
    else if (in_any_view(actor->pos, Int2Fx(-32), FALSE))
        FLAG_ON(actor, FLG_CHEEP_ACTIVE);

    if (ANY_FLAG(actor, FLG_CHEEP_OVERLAP)) {
        if (!touching_solid(Radd(actor->box, actor->pos), SOL_SOLID))
            FLAG_OFF(actor, FLG_CHEEP_OVERLAP);
    } else if (touching_solid(Radd(actor->box, actor->pos), SOL_SOLID)) {
        TOGGLE_FLAG(actor, FLG_X_FLIP);
        FLAG_ON(actor, FLG_CHEEP_OVERLAP);
    }
}

static void draw_blue(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/cheep/blue/%i", (VAL(actor, CHEEP_FRAME) / 100) % 2), FALSE);
}

static void draw_dead_blue(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/cheep/blue/dead", FALSE);
}

static void collide_blue(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        maybe_hit_player(actor, from);
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

    case ACT_KOOPA_SHELL: {
        hit_shell(actor, from);
        break;
    }
    }
}

const ActorTable TAB_CHEEP_BLUE = {
    .load = load_blue,
    .create = create_blue,
    .tick = tick_blue,
    .draw = draw_blue,
    .draw_dead = draw_dead_blue,
    .collide = collide_blue,
};
