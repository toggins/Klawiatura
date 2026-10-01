#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_water.h"

enum {
    VAL_CHEEP_SPEED,
    VAL_CHEEP_ANGLE,
    VAL_CHEEP_FRAME,

    VAL_CHEEP_SPAWN = 0,
    VAL_CHEEP_SPAWN_START_X,
    VAL_CHEEP_SPAWN_START_Y,
    VAL_CHEEP_SPAWN_END_X,
    VAL_CHEEP_SPAWN_END_Y,
    VAL_CHEEP_SPAWN_Y,
};

#define FLG_CHEEP_ACTIVE CUSTOM_FLAG(0)
#define FLG_CHEEP_TOUCHED_WATER CUSTOM_FLAG(1)
#define FLG_CHEEP_OVERLAP CUSTOM_FLAG(2)
#define FLG_CHEEP_JUMP CUSTOM_FLAG(3)
#define FLG_CHEEP_7_3 CUSTOM_FLAG(4)
#define FLG_CHEEP_CAN_SPAWN CUSTOM_FLAG(5)

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
    VAL(actor, CHEEP_SPAWN_Y) = actor->pos.y;
}

static void tick_spawner_underwater(GameActor* actor) {
    const GameState* game_state = gamestate();

    Bool allow_spawn = TRUE;
    const GameActor* water = get_actor(game_state->water);
    if (water != NULL && ANY_FLAG(actor, FLG_CHEEP_7_3)) {
        move_actor(actor, (FVec2){actor->pos.x, water->pos.y + (VAL(actor, CHEEP_SPAWN_Y) - VAL(water, WATER_Y))});

        const Fixed dy = actor->last_pos.y - actor->pos.y;
        VAL(actor, CHEEP_SPAWN_START_Y) += dy;
        VAL(actor, CHEEP_SPAWN_END_Y) += dy;

        allow_spawn = in_any_view(water->pos, Int2Fx(-128), VEF_IGNORE_X);
    }

    if (ANY_FLAG(actor, FLG_CHEEP_CAN_SPAWN)
        && (VAL(actor, CHEEP_SPAWN) <= 1 || (game_state->time % VAL(actor, CHEEP_SPAWN)) == 0) && allow_spawn)
    {
        const PlayerID n = gamecontext()->num_players;
        if (get_num_actors(ACT_CHEEP) < ((ActorID)n * 10)) {
            Fixed edge = (actor->vel.x > Fx0) ? FxUpper : FxLower;
            for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
                const GamePlayer* player = get_player(i);
                if (player == NULL)
                    continue;

                if ((VAL(actor, CHEEP_SPAWN_START_X) == VAL(actor, CHEEP_SPAWN_END_X)
                        || (player->pos.x > VAL(actor, CHEEP_SPAWN_START_X)
                            && player->pos.x < VAL(actor, CHEEP_SPAWN_END_X)))
                    && (VAL(actor, CHEEP_SPAWN_START_Y) == VAL(actor, CHEEP_SPAWN_END_Y)
                        || (player->pos.y > VAL(actor, CHEEP_SPAWN_START_Y)
                            && player->pos.y < VAL(actor, CHEEP_SPAWN_END_Y))))
                {
                    const Fixed px = get_player_view(player).x;
                    if ((actor->vel.x > Fx0 && px < edge) || (actor->vel.x < Fx0 && px > edge))
                        edge = px;
                }
            }

            if (edge > FxLower && edge < FxUpper) {
                FVec2 cpos = actor->pos;
                cpos.x += edge;
                cpos.y += Int2Fx(rng(300));

                GameActor* cheep = create_actor(ACT_CHEEP, cpos);
                if (cheep != NULL) {
                    cheep->vel.x = actor->vel.x;
                    if (actor->vel.x > Fx0) {
                        VAL(cheep, CHEEP_SPEED) = actor->vel.x;
                    } else {
                        VAL(cheep, CHEEP_SPEED) = -actor->vel.x;
                        VAL(cheep, CHEEP_ANGLE) = FxPi;
                        FLAG_ON(cheep, FLG_X_FLIP);
                    }

                    if (ANY_FLAG(actor, FLG_CHEEP_7_3))
                        FLAG_ON(cheep, FLG_CHEEP_JUMP | FLG_CHEEP_7_3);
                }

                FLAG_OFF(actor, FLG_CHEEP_CAN_SPAWN);
            }
        }
    }

    const GameActor* nearest = NULL;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER
            && (VAL(actor, CHEEP_SPAWN_START_X) == VAL(actor, CHEEP_SPAWN_END_X)
                || (pawn->pos.x > VAL(actor, CHEEP_SPAWN_START_X) && pawn->pos.x < VAL(actor, CHEEP_SPAWN_END_X)))
            && (VAL(actor, CHEEP_SPAWN_START_Y) == VAL(actor, CHEEP_SPAWN_END_Y)
                || (pawn->pos.y > VAL(actor, CHEEP_SPAWN_START_Y) && pawn->pos.y < VAL(actor, CHEEP_SPAWN_END_Y)))
            && (nearest == NULL || (actor->vel.x > Fx0 && pawn->pos.x < nearest->pos.x)
                || (actor->vel.x < Fx0 && pawn->pos.x > nearest->pos.x)))
        {
            nearest = pawn;
        }
    }

    if (nearest != NULL
        && (((game_state->time % 10) == 0
                && ((actor->vel.x < Fx0 && nearest->vel.x < 16384) || (actor->vel.x > Fx0 && nearest->vel.x > -16384)))
            || ((game_state->time % 4) == 0
                && ((actor->vel.x < Fx0 && nearest->vel.x >= 16384)
                    || (actor->vel.x > Fx0 && nearest->vel.x <= -16384)))))
    {
        FLAG_ON(actor, FLG_CHEEP_CAN_SPAWN);
    }
}

static void tick_spawner_jump(GameActor* actor) {
    if (VAL(actor, CHEEP_SPAWN) > 1 && (gamestate()->time % VAL(actor, CHEEP_SPAWN)) > 0)
        return;

    const PlayerID n = gamecontext()->num_players;
    if (get_num_actors(ACT_CHEEP) >= ((ActorID)n * (ANY_FLAG(actor, FLG_CHEEP_7_3) ? 20 : 10)))
        return;

    const GamePlayer* target = NULL;
    for (PlayerID i = 0; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER
            || (VAL(actor, CHEEP_SPAWN_START_X) != VAL(actor, CHEEP_SPAWN_END_X)
                && (pawn->pos.x <= VAL(actor, CHEEP_SPAWN_START_X) || pawn->pos.x >= VAL(actor, CHEEP_SPAWN_END_X))))
        {
            continue;
        }

        if (target == NULL || (actor->vel.x > Fx0 && pawn->pos.x < target->pos.x)
            || (actor->vel.x < Fx0 && pawn->pos.x > target->pos.x))
        {
            target = player;
        }
    }

    if (target == NULL)
        return;

    if (ANY_FLAG(actor, FLG_CHEEP_7_3)) {
        if (rng(30) >= 2)
            return;
    } else if (rng(20) != 10) {
        return;
    }

    FVec2 cpos = actor->pos;
    cpos.x += get_player_view(target).x;
    if (actor->vel.x > Fx0)
        cpos.x += Int2Fx(rng(100));
    else
        cpos.x -= Int2Fx(rng(100));
    cpos.y += Int2Fx(22);

    GameActor* cheep = create_actor(ACT_CHEEP, cpos);
    if (cheep == NULL)
        return;

    if (actor->vel.x > Fx0) {
        cheep->vel.x = Fx1 + Int2Fx(rng(5));
    } else {
        cheep->vel.x = -Fx1 - Int2Fx(rng(5));
        FLAG_ON(cheep, FLG_X_FLIP);
    }
    cheep->vel.y = Int2Fx(-5) - Int2Fx(rng(7));

    FLAG_ON(cheep, actor->flags & (FLG_CHEEP_JUMP | FLG_CHEEP_7_3));
}

static void tick_spawner(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_CHEEP_JUMP))
        tick_spawner_jump(actor);
    else
        tick_spawner_underwater(actor);
}

const ActorTable TAB_CHEEP_SPAWNER = {
    .load = load_spawner,
    .create = create_spawner,
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
        const GameState* game_state = gamestate();

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
            create_actor(ACT_WATER_SPLASH, actor->pos);
            FLAG_ON(actor, FLG_CHEEP_TOUCHED_WATER);
        }

        if (ANY_FLAG(actor, FLG_CHEEP_7_3)) {
            if ((game_state->time % 5) == 0)
                actor->vel.y += Int2Fx(rng(2));
        } else {
            actor->vel.y += 13107;
        }

        if (!in_any_view(actor->pos, Int2Fx(-96), VEF_ALL))
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    const GameState* game_state = gamestate();
    if ((game_state->time % 50) == 0) {
        if (ANY_FLAG(actor, FLG_X_FLIP))
            move_cheep(actor, 193019 + (rng(3) * 12868));
        else
            move_cheep(actor, 12868 - (rng(3) * 12868));
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

    case ACT_BULLET_PROJECTILE: {
        hit_bullet(actor, from, 100);
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
    load_sprite("enemies/cheep/blue/dead", AKL_NEVER);
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
    case ACT_PLAYER:
        maybe_hit_player(actor, from);
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
    case ACT_BULLET_PROJECTILE:
        hit_bullet(actor, from, 500);
        break;
    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL:
        hit_shell(actor, from);
        break;
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
    case ACT_BULLET_PROJECTILE:
        hit_bullet(actor, from, 500);
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

/* =================
   GREEN CHEEP CHEEP
   ================= */

static void load_green() {
    load_sprite_num("enemies/cheep/green/%u", 2, AKL_NEVER);
    load_sprite("enemies/cheep/green/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_green(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;

    VAL(actor, CHEEP_SPEED) = 122880;
    VAL(actor, CHEEP_ANGLE) = 205887;
}

static void tick_green(GameActor* actor) {
    VAL(actor, CHEEP_FRAME) += 7;

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        move_cheep(actor, VAL(actor, CHEEP_ANGLE));
    else
        actor->vel.x = actor->vel.y = Fx0;

    if ((gamestate()->time % 50) == 0 && (actor->vel.x != Fx0 || actor->vel.y != Fx0)) {
        if (ANY_FLAG(actor, FLG_X_FLIP))
            move_cheep(actor, 193019 + (rng(3) * 12868));
        else
            move_cheep(actor, 12868 - (rng(3) * 12868));
    }

    const GameActor* water = get_actor(gamestate()->water);
    if (water != NULL && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16))
        && (actor->pos.y + actor->box.end.y) > water->pos.y)
    {
        VAL(actor, CHEEP_ANGLE) = ANY_FLAG(actor, FLG_X_FLIP) ? 218755 : 398907;
        if (actor->vel.x != Fx0 || actor->vel.y != Fx0)
            move_cheep(actor, VAL(actor, CHEEP_ANGLE));
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));
}

static void draw_green(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/cheep/green/%i", (VAL(actor, CHEEP_FRAME) / 50) % 2), FALSE);
}

static void draw_dead_green(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/cheep/green/dead", FALSE);
}

const ActorTable TAB_CHEEP_GREEN = {
    .load = load_green,
    .create = create_green,
    .tick = tick_green,
    .draw = draw_green,
    .draw_dead = draw_dead_green,
    .collide = collide,
};
