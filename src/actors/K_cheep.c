#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

enum {
    VAL_CHEEP_SPEED,
    VAL_CHEEP_ANGLE,
    VAL_CHEEP_FRAME,

    VAL_CHEEP_SPAWN = 0,
    VAL_CHEEP_SPAWN_START_X,
    VAL_CHEEP_SPAWN_START_Y,
    VAL_CHEEP_SPAWN_END_X,
    VAL_CHEEP_SPAWN_END_Y,
};

#define FLG_CHEEP_ACTIVE CUSTOM_FLAG(0)
#define FLG_CHEEP_TOUCHED_WATER CUSTOM_FLAG(1)
#define FLG_CHEEP_OVERLAP CUSTOM_FLAG(2)
#define FLG_CHEEP_JUMP CUSTOM_FLAG(3)

static void move_cheep(GameActor* actor, Fixed angle) {
    actor->vel.x = Fmul(VAL(actor, CHEEP_SPEED), Fcos(angle));
    actor->vel.y = Fmul(VAL(actor, CHEEP_SPEED), -Fsin(angle));

    if (actor->vel.x < Fx0)
        FLAG_ON(actor, FLG_X_FLIP);
    else if (actor->vel.x > Fx0)
        FLAG_OFF(actor, FLG_X_FLIP);

    VAL(actor, CHEEP_ANGLE) = angle;
}

/* ===================
   CHEEP CHEEP SPAWNER
   =================== */

static void load_spawner() {
    load_actor(ACT_CHEEP);
}

static void create_spawner(GameActor* actor) {
    actor->vel.x = -73728;

    VAL(actor, CHEEP_SPAWN) = 100;
}

static void tick_spawner(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (ANY_FLAG(actor, FLG_CHEEP_JUMP)
        && ((VAL(actor, CHEEP_SPAWN) > 1 && (game_state->time % VAL(actor, CHEEP_SPAWN)) > 0) || rng(20) != 10))
    {
        return;
    }

    const GameActor* water = get_actor(game_state->water);
    if (!ANY_FLAG(actor, FLG_CHEEP_JUMP)
        && (water == NULL || below_nearest_bounds(water->pos, Fx0)
            || (VAL(actor, CHEEP_SPAWN) > 1 && (game_state->time % VAL(actor, CHEEP_SPAWN)) > 0)))
    {
        return;
    }

    const PlayerID n = gamecontext()->num_players;
    if (get_num_actors(ACT_CHEEP) >= (10 * n))
        return;

    Bool found = FALSE;
    Fixed edge = (actor->vel.x < Fx0) ? FxLower : FxUpper;
    for (PlayerID i = 0; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        if ((VAL(actor, CHEEP_SPAWN_START_X) != VAL(actor, CHEEP_SPAWN_END_X)
                && (player->pos.x <= VAL(actor, CHEEP_SPAWN_START_X) || player->pos.x >= VAL(actor, CHEEP_SPAWN_END_X)))
            || (VAL(actor, CHEEP_SPAWN_START_Y) != VAL(actor, CHEEP_SPAWN_END_Y)
                && (player->pos.y <= VAL(actor, CHEEP_SPAWN_START_Y)
                    || player->pos.y >= VAL(actor, CHEEP_SPAWN_END_Y))))
        {
            continue;
        }

        const FVec2 ppos = get_player_view(player);
        edge = (actor->vel.x < Fx0) ? Fmax(edge, ppos.x) : Fmin(edge, ppos.x);
        found = TRUE;
    }

    if (!found)
        return;

    FVec2 cpos = actor->pos;
    cpos.x += edge;
    if (ANY_FLAG(actor, FLG_CHEEP_JUMP)) {
        cpos.x -= Int2Fx(rng(100));
    } else {
        cpos = Vsub(cpos, water->pos);
        cpos.y += water->pos.y + Int2Fx(rng(300));
    }

    GameActor* cheep = create_actor(ACT_CHEEP, cpos);
    if (cheep == NULL)
        return;

    if (ANY_FLAG(actor, FLG_CHEEP_JUMP)) {
        cheep->vel.x = Fx1 + Int2Fx(rng(5));
        cheep->vel.y = Int2Fx(-5) - Int2Fx(rng(7));

        if (actor->vel.x < 0) {
            cheep->vel.x = -cheep->vel.x;
            FLAG_ON(cheep, FLG_X_FLIP);
        }

        FLAG_ON(cheep, FLG_CHEEP_JUMP);
        return;
    }

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

    actor->depth = Fx1;
}

static void tick(GameActor* actor) {
    VAL(actor, CHEEP_FRAME) += ANY_FLAG(actor, FLG_CHEEP_JUMP) ? 7 : 1;

    if (ANY_FLAG(actor, FLG_CHEEP_JUMP))
        move_actor(actor, Vadd(actor->pos, actor->vel));

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (ANY_FLAG(actor, FLG_CHEEP_JUMP)) {
        const GameActor* water = get_actor(gamestate()->water);
        if (ANY_FLAG(actor, FLG_CHEEP_TOUCHED_WATER)) {
            if (water == NULL || (actor->pos.y + actor->box.end.y) <= water->pos.y
                || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
            {
                FLAG_OFF(actor, FLG_CHEEP_TOUCHED_WATER);
            }
        } else if (water != NULL && (actor->pos.y + actor->box.end.y) > water->pos.y
                   && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16)))
        {
            create_actor(ACT_WATER_SPLASH, actor->pos);
            FLAG_ON(actor, FLG_CHEEP_TOUCHED_WATER);
        }

        actor->vel.y += 13107;

        if (!in_any_view(actor->pos, Int2Fx(-96), VEF_ALL))
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    const GameState* game_state = gamestate();
    if ((game_state->time % 50) == 0) {
        move_cheep(
            actor, (Fabs(VAL(actor, CHEEP_ANGLE)) > FxPi2) ? (193019 + (rng(3) * 12868)) : (12868 - (rng(3) * 12868)));
    }

    Fixed edge = (actor->vel.x > Fx0) ? FxLower : FxUpper;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player != NULL) {
            const FVec2 ppos = get_player_view(player);
            edge = (actor->vel.x > Fx0) ? Fmax(edge, ppos.x + F_SCREEN_WIDTH + Int2Fx(100))
                                        : Fmin(edge, ppos.x - Int2Fx(100));
        }
    }
    if ((actor->vel.x > Fx0 && actor->pos.x > edge) || (actor->vel.x <= Fx0 && actor->pos.x < edge)) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
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
        move_cheep(actor, 218755);
        FLAG_ON(actor, FLG_CHEEP_TOUCHED_WATER);
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_CHEEP_JUMP) ? fmt("enemies/cheep/alt/%i", (VAL(actor, CHEEP_FRAME) / 50) % 2)
                                        : fmt("enemies/cheep/%i", (VAL(actor, CHEEP_FRAME) / 2) % 24),
        FALSE);
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
        if (ANY_FLAG(actor, FLG_CHEEP_JUMP)) {
            if (check_stomp(actor, from, Int2Fx(-10), 100, TRUE))
                kill_enemy(actor, from, FALSE);
        } else {
            maybe_hit_player(actor, from);
        }

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

    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL: {
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

    actor->depth = Fx1;
}

static void tick_blue(GameActor* actor) {
    VAL(actor, CHEEP_FRAME) += 9;

    if (ANY_FLAG(actor, FLG_CHEEP_ACTIVE))
        move_actor(actor, Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? -81920 : 81920, Fx0}));
    else if (in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
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

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 500);
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

const ActorTable TAB_CHEEP_BLUE = {
    .load = load_blue,
    .create = create_blue,
    .tick = tick_blue,
    .draw = draw_blue,
    .draw_dead = draw_dead_blue,
    .collide = collide_blue,
};

/* =================
   SPIKY CHEEP CHEEP
   ================= */

static void load_spiky() {
    load_sprite_num("enemies/cheep/spiky/%u", 2, AKL_NEVER);
    load_sprite("enemies/cheep/spiky/dead", AKL_NEVER);
    load_sound("bump", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_spiky(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(15);
    actor->box.end.y = Int2Fx(12);

    actor->depth = Fx1;
}

static void tick_spiky(GameActor* actor) {
    VAL(actor, CHEEP_FRAME) += 4;

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        FLAG_ON(actor, FLG_CHEEP_ACTIVE);

    const GameState* game_state = gamestate();
    if (ANY_FLAG(actor, FLG_CHEEP_ACTIVE) && (game_state->time % 50) == 0) {
        VAL(actor, CHEEP_SPEED) = 81920;
        move_cheep(actor,
            Fmul(Ffloor(Fdiv(Vtheta(actor->pos, Vadd(nearest_player_pos(actor->pos), (FVec2){Fx0, Int2Fx(-14)})), 12868)
                        + FxHalf),
                12868));
    }

    const GameActor* water = get_actor(game_state->water);
    if (water == NULL || actor->pos.y < water->pos.y)
        move_cheep(actor, 270227 + (rng(7) * 12868));

    if (!ANY_FLAG(actor, FLG_CHEEP_ACTIVE))
        actor->vel.x = actor->vel.y = Fx0;

    if (in_any_view(actor->pos, Int2Fx(256), VEF_ALL))
        FLAG_OFF(actor, FLG_CHEEP_ACTIVE);

    move_actor(actor, Vadd(actor->pos, actor->vel));
}

static void draw_spiky(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/cheep/spiky/%i", (VAL(actor, CHEEP_FRAME) / 25) % 2), FALSE);
}

static void draw_dead_spiky(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/cheep/spiky/dead", FALSE);
}

static void collide_spiky(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;
    case ACT_PLAYER:
        maybe_hit_player(actor, from);
        break;
    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL:
        hit_shell(actor, from);
        break;
    case ACT_FIREBALL_PROJECTILE:
        block_fireball(from);
        break;
    case ACT_BEETROOT_PROJECTILE:
        block_beetroot(from);
        break;
    case ACT_HAMMER_PROJECTILE:
        hit_hammer(actor, from, 500);
        break;
    }
}

const ActorTable TAB_CHEEP_SPIKY = {
    .load = load_spiky,
    .create = create_spiky,
    .tick = tick_spiky,
    .draw = draw_spiky,
    .draw_dead = draw_dead_spiky,
    .collide = collide_spiky,
};
