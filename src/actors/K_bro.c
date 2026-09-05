#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"

enum {
    VAL_BRO_TYPE,

    VAL_BRO_MOVE,
    VAL_BRO_B,
    VAL_BRO_C,
    VAL_BRO_D,
    VAL_BRO_E,
    VAL_BRO_JUMP,
    VAL_BRO_TO,
    VAL_BRO_THROW,

    VAL_BRO_FRAME,
};

#define FLG_BRO_ACTIVE CUSTOM_FLAG(0)
#define FLG_BRO_BOTTOM CUSTOM_FLAG(2)
#define FLG_BRO_TOP CUSTOM_FLAG(3)

#define FLG_BRO_LAYER_TOP CUSTOM_FLAG(0)

/* ===
   BRO
   === */

static void load() {
    load_sprite_num("enemies/bro/%u", 2, AKL_NEVER);
    load_sprite_num("enemies/bro/hammer/%u", 2, AKL_NEVER);
    load_sprite("enemies/bro/dead", AKL_NEVER);
    load_sound("hammer", AKL_NEVER);
    load_actor(ACT_HAMMER_PROJECTILE);
}

static void load_special(const GameActor* actor) {
    switch (VAL(actor, BRO_TYPE)) {
    default:
        break;

    case ACT_FIREBALL_PROJECTILE: {
        load_sprite_num("enemies/bro/fire/%u", 2, AKL_NEVER);
        load_sound("fire", AKL_NEVER);
        break;
    }

    case ACT_SILVER_HAMMER_PROJECTILE: {
        load_sprite_num("enemies/bro/silver/%u", 2, AKL_NEVER);
        break;
    }
    }

    load_actor(VAL(actor, BRO_TYPE));
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-14);
    actor->box.start.y = Int2Fx(-47);
    actor->box.end.x = Int2Fx(14);
    actor->box.end.y = Fx1;

    VAL(actor, BRO_TYPE) = ACT_HAMMER_PROJECTILE;

    increase_ambush();
}

static void cleanup(GameActor* actor) {
    (void)actor;

    decrease_ambush();
}

static void tick(GameActor* actor) {
    VAL(actor, BRO_FRAME) += 7;

    // EVENTS FROM "Level 3 - 1"

    // 110
    const LevelInfo* level_info = levelinfo();
    if (actor->pos.y > (level_info->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    // 769, 770
    const FVec2 ppos = nearest_player_pos(actor->pos);
    if (actor->pos.x > ppos.x)
        FLAG_ON(actor, FLG_X_FLIP);
    if (actor->pos.x < ppos.x)
        FLAG_OFF(actor, FLG_X_FLIP);

    // 771, 772
    if (VAL(actor, BRO_MOVE) < 0) {
        ++VAL(actor, BRO_MOVE);
        move_actor(actor, Vadd(actor->pos, (FVec2){Int2Fx(-2), Fx0}));
    }
    if (VAL(actor, BRO_MOVE) > 0) {
        --VAL(actor, BRO_MOVE);
        move_actor(actor, Vadd(actor->pos, (FVec2){Int2Fx(2), Fx0}));
    }

    if (ANY_FLAG(actor, FLG_BRO_ACTIVE)) {
        if (VAL(actor, BRO_MOVE) == 0)
            VAL(actor, BRO_B) += VAL(actor, BRO_E);
    } else if (in_any_view(actor->pos, Int2Fx(-32), FALSE)) {
        FLAG_ON(actor, FLG_BRO_ACTIVE);
        VAL(actor, BRO_B) = 101;
        VAL(actor, BRO_C) = VAL(actor, BRO_E) = 1;
    }

    if (VAL(actor, BRO_B) > -1) {
        if (VAL(actor, BRO_C) == 1) {
            VAL(actor, BRO_B) = 0;
            VAL(actor, BRO_MOVE) = -32 - VAL(actor, BRO_D);
            VAL(actor, BRO_C) = 2;
        } else if (VAL(actor, BRO_C) == 5) {
            VAL(actor, BRO_B) = 0;
            VAL(actor, BRO_MOVE) = 32 + VAL(actor, BRO_D);
            VAL(actor, BRO_C) = 6;
        }
    }

    if (VAL(actor, BRO_MOVE) == 0) {
        switch (VAL(actor, BRO_C)) {
        default:
            break;

        case 2:
        case 4:
        case 6: {
            ++VAL(actor, BRO_C);
            break;
        }

        case 8: {
            VAL(actor, BRO_C) = 1;
            VAL(actor, BRO_D) = -rng(64);
            VAL(actor, BRO_E) = 1 + rng(5);

            break;
        }
        }
    }

    if (VAL(actor, BRO_B) > 100) {
        if (VAL(actor, BRO_C) == 3) {
            VAL(actor, BRO_B) = 0;
            VAL(actor, BRO_MOVE) = 32 + VAL(actor, BRO_D);
            VAL(actor, BRO_C) = 4;
        } else if (VAL(actor, BRO_C) == 7) {
            VAL(actor, BRO_B) = 0;
            VAL(actor, BRO_MOVE) = -32 - VAL(actor, BRO_D);
            VAL(actor, BRO_C) = 8;
        }
    }

    actor->vel.y += 13107;

    const GameState* game_state = gamestate();
    if ((game_state->time % 10) == 0 && ANY_FLAG(actor, FLG_BRO_ACTIVE) && VAL(actor, BRO_JUMP) == 0) {
        FLAG_OFF(actor, FLG_BRO_BOTTOM | FLG_BRO_TOP);
        collide_actor(actor);

        switch (rng(20)) {
        default:
            break;

        case 5: {
            if (ANY_FLAG(actor, FLG_BRO_BOTTOM | FLG_BRO_TOP)) {
                VAL(actor, BRO_JUMP) = 1;
                actor->vel.y = Int2Fx(-8);
            }

            break;
        }

        case 15: {
            if (ANY_FLAG(actor, FLG_BRO_TOP)) {
                VAL(actor, BRO_JUMP) = 2;
                VAL(actor, BRO_TO) = actor->pos.y + Int2Fx(32) - actor->box.start.y;
                actor->vel.y = Int2Fx(-3);
            }

            break;
        }
        }
    }

    if (touching_solid(Radd(actor->box, Vadd(actor->pos, actor->vel)), SOL_SOLID | SOL_TOP)
        && (VAL(actor, BRO_JUMP) == 0 || (VAL(actor, BRO_JUMP) == 1 && actor->vel.y > Fx0)
            || (VAL(actor, BRO_JUMP) == 2 && actor->pos.y > VAL(actor, BRO_TO))))
    {
        Fixed i = actor->vel.y;
        while (i >= Fx1) {
            const FVec2 move
                = Vadd(actor->pos, (FVec2){Fx0, Int2Fx((Sint32)(actor->vel.y > Fx0) - (Sint32)(actor->vel.y < Fx0))});
            if (touching_solid(Radd(actor->box, move), SOL_SOLID | SOL_TOP))
                break;

            move_actor(actor, move);
            i -= Fx1;
        }

        actor->vel.y = Fx0;
        VAL(actor, BRO_JUMP) = 0;
    } else {
        move_actor(actor, Vadd(actor->pos, actor->vel));
    }

    if (((game_state->time * 2) % 5) <= 1 && ANY_FLAG(actor, FLG_BRO_ACTIVE)
        && in_any_view(actor->pos, Int2Fx(-32), FALSE) && rng(20) == 10)
    {
        VAL(actor, BRO_FRAME) = 0;
        ++VAL(actor, BRO_THROW);
    }

    if (VAL(actor, BRO_THROW) > 0)
        ++VAL(actor, BRO_THROW);

    if (VAL(actor, BRO_THROW) > levelinfo()->bro_throw) {
        VAL(actor, BRO_FRAME) = VAL(actor, BRO_THROW) = 0;

        switch (VAL(actor, BRO_TYPE)) {
        default:
            break;

        case ACT_HAMMER_PROJECTILE: {
            GameActor* hammer = create_actor(ACT_HAMMER_PROJECTILE,
                Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-9) : Int2Fx(9), Int2Fx(-20)}));
            if (hammer != NULL) {
                FLAG_ON(hammer, actor->flags & FLG_X_FLIP);
                hammer->vel.x = Fx1 + Int2Fx(rng(5));
                if (ANY_FLAG(hammer, FLG_X_FLIP))
                    hammer->vel.x = -hammer->vel.x;
                hammer->vel.y = Int2Fx(-6) - Int2Fx(rng(5));
            }

            play_state_sound("hammer", PLAY_POS, A_ACTOR(actor));
            break;
        }

        case ACT_FIREBALL_PROJECTILE: {
            play_state_sound("fire", PLAY_POS, A_ACTOR(actor));
            break;
        }

        case ACT_SILVER_HAMMER_PROJECTILE: {
            play_state_sound("hammer", PLAY_POS, A_ACTOR(actor));
            break;
        }
        }
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt((VAL(actor, BRO_THROW) > 0) ? "enemies/bro/hammer/%i" : "enemies/bro/%i", (VAL(actor, BRO_FRAME) / 50) % 2),
        FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/bro/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (check_stomp(actor, from, Int2Fx(-16), 200, TRUE))
            kill_enemy(actor, from, FALSE);

        break;
    }

    case ACT_BLOCK_BUMP: {
        hit_bump(actor, from, 200);
        break;
    }

    case ACT_KOOPA_SHELL: {
        hit_shell(actor, from);
        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        hit_fireball(actor, from, 200);
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        hit_beetroot(actor, from, 200);
        break;
    }
    }
}

const ActorTable TAB_BRO = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};

/* =========
   BRO LAYER
   ========= */

static void create_layer(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void collide_layer(GameActor* actor, GameActor* from) {
    if (from->type == ACT_BRO) {
        if (ANY_FLAG(actor, FLG_BRO_LAYER_TOP))
            FLAG_ON(from, FLG_BRO_TOP);
        FLAG_ON(from, FLG_BRO_BOTTOM);
    }
}

const ActorTable TAB_BRO_LAYER = {
    .create = create_layer,
    .collide = collide_layer,
};
