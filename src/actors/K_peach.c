#include "K_audio.h"
#include "K_game.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_video.h"

typedef Uint8 PeachAnimations;
enum {
    PA_IDLE,
    PA_WALK,
    PA_THROW,
};

enum {
    VAL_PEACH_THROWS,
    VAL_PEACH_ANIMATION,
    VAL_PEACH_FRAME,

    VAL_CAGE_STATE = 0,
    VAL_CAGE_ANGLE,
};

#define FLG_CAGE_BROKEN CUSTOM_FLAG(0)

/* =====
   PEACH
   ===== */

static void load() {
    load_sprite_num("markers/peach/%u", 2, AKL_NEVER);
    load_sprite_num("markers/peach/throw/%u", 3, AKL_NEVER);
    load_sprite_num("markers/peach/walk/%u", 2, AKL_NEVER);
    load_font("hud", AKL_NEVER);
    load_actor(ACT_SUPER_MUSHROOM);
    load_actor(ACT_FIRE_FLOWER);
}

static void create(GameActor* actor) {
    actor->depth = Int2Fx(11);
}

static void pre_tick(GameActor* actor) {
    switch (VAL(actor, PEACH_ANIMATION)) {
    default: {
        VAL(actor, PEACH_FRAME) += 11;
        break;
    }

    case PA_WALK: {
        VAL(actor, PEACH_FRAME) += 44;
        break;
    }

    case PA_THROW: {
        VAL(actor, PEACH_FRAME) += 11;
        if (VAL(actor, PEACH_FRAME) >= 1300) {
            VAL(actor, PEACH_ANIMATION) = PA_IDLE;
            VAL(actor, PEACH_FRAME) = 0;
        }

        break;
    }
    }
}

static void tick(GameActor* actor) {
    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (actor->vel.x > Fx0 && VAL(actor, PEACH_ANIMATION) != PA_WALK) {
        VAL(actor, PEACH_ANIMATION) = PA_WALK;
        VAL(actor, PEACH_FRAME) = 0;
        actor->depth = Int2Fx(9);
    }

    const GameSequence* sequence = get_sequence();
    if (actor->pos.x > (levelinfo()->size.x + Int2Fx(360)) && sequence->type == GS_RESCUE)
        gamestate()->flags |= GF_END;

    if (VAL(actor, PEACH_THROWS) < 4) {
        const GameState* game_state = gamestate();
        if (game_state->time > 150 && (game_state->time % 250) == 0 && get_num_actors(ACT_FIRE_FLOWER) <= 0) {
            for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
                const GamePlayer* player = get_player(i);
                if (player == NULL || player->powerup != POW_SUPER_MUSHROOM)
                    continue;

                const GameActor* pawn = get_actor(player->actor);
                if (pawn == NULL || pawn->type != ACT_PLAYER)
                    continue;

                GameActor* flower = create_actor(ACT_FIRE_FLOWER, Vadd(actor->pos, (FVec2){Int2Fx(12), Int2Fx(-45)}));
                if (flower != NULL)
                    flower->player = i;

                VAL(actor, PEACH_ANIMATION) = PA_THROW;
                VAL(actor, PEACH_FRAME) = 0;
                ++VAL(actor, PEACH_THROWS);

                break;
            }
        }

        if (game_state->time > 50 && (game_state->time % 50) == 0 && get_num_actors(ACT_SUPER_MUSHROOM) <= 0) {
            for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
                const GamePlayer* player = get_player(i);
                if (player == NULL || player->powerup != POW_NONE)
                    continue;

                const GameActor* pawn = get_actor(player->actor);
                if (pawn == NULL || pawn->type != ACT_PLAYER)
                    continue;

                GameActor* mushroom
                    = create_actor(ACT_SUPER_MUSHROOM, Vadd(actor->pos, (FVec2){Int2Fx(14), Int2Fx(-48)}));
                if (mushroom != NULL)
                    mushroom->vel.x = Int2Fx(2);

                VAL(actor, PEACH_ANIMATION) = PA_THROW;
                VAL(actor, PEACH_FRAME) = 0;
                ++VAL(actor, PEACH_THROWS);

                break;
            }
        }
    }

    if (sequence->type == GS_RESCUE) {
        if (actor->pos.y < Int2Fx(352))
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Fx1}));

        if (sequence->state > 0 && actor->vel.x < Int2Fx(6))
            actor->vel.x = Fmin(actor->vel.x + 14564, Int2Fx(6));
    }
}

static void draw(const GameActor* actor) {
    batch_reset();

    const char* sprite = NULL;
    switch (VAL(actor, PEACH_ANIMATION)) {
    default:
        sprite = fmt("markers/peach/%i", (VAL(actor, PEACH_FRAME) / 100) % 2);
        break;
    case PA_WALK:
        sprite = fmt("markers/peach/walk/%i", (VAL(actor, PEACH_FRAME) / 100) % 2);
        break;
    case PA_THROW:
        sprite = (VAL(actor, PEACH_FRAME) >= 300) ? "markers/peach/throw/2"
                                                  : fmt("markers/peach/throw/%i", VAL(actor, PEACH_FRAME) / 100);
        break;
    }

    draw_actor(actor, sprite, FALSE);

    if (get_sequence()->type != GS_RESCUE) {
        const GameState* game_state = gamestate();
        if (game_state->time <= 25 || ((game_state->time - 25) % 30) < 15) {
            batch_offset(B_F3_XY(-45.f, 40.f));
            batch_string("hud", 16.f, LFMT("hud.help"));
        }
    }
}

const ActorTable TAB_PEACH = {
    .load = load,
    .create = create,
    .pre_tick = pre_tick,
    .tick = tick,
    .draw = draw,
};

/* ============
   PEACH'S CAGE
   ============ */

static void create_cage_shard(FVec2 pos) {
    GameActor* shard = create_actor(ACT_PEACH_CAGE_SHARD, pos);
    if (shard == NULL)
        return;

    shard->vel.x = Int2Fx(rng(3));
    shard->vel.x -= Int2Fx(rng(3));
    shard->vel.y = Int2Fx(-5) - Int2Fx(rng(6));
}

static void load_cage() {
    load_sprite("markers/cage", AKL_NEVER);
    load_sprite("markers/cage/broken", AKL_NEVER);
    load_sprite("markers/cage/chain", AKL_NEVER);
    load_sound("break", AKL_NEVER);
    load_actor(ACT_PEACH_CAGE_SHARD);
}

static void create_cage(GameActor* actor) {
    actor->box.start.x = Int2Fx(-35);
    actor->box.start.y = Int2Fx(-97);
    actor->box.end.x = Int2Fx(35);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(10);
}

static void tick_cage(GameActor* actor) {
    GameSequence* sequence = get_sequence();
    if (sequence->type != GS_RESCUE)
        return;

    if (actor->pos.y < Int2Fx(365)) {
        move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Fx1}));

        if (actor->pos.y < Int2Fx(365))
            return;
    }

    GameActor* hero = NULL;
    const GamePlayer* player = get_player(sequence->activator);
    if (player != NULL) {
        hero = get_actor(player->actor);
        if (hero == NULL || hero->type != ACT_PLAYER)
            hero = NULL;
    }

    if (hero == NULL)
        return;

    if (!ANY_FLAG(actor, FLG_CAGE_BROKEN)) {
        if (actor->pos.x > hero->pos.x) {
            if (hero->vel.x < Int2Fx(7))
                hero->vel.x = Fmin(hero->vel.x + 16384, Int2Fx(7));
            FLAG_OFF(hero, FLG_X_FLIP);
        } else if (actor->pos.x < hero->pos.x) {
            if (hero->vel.x > Int2Fx(-7))
                hero->vel.x = Fmax(hero->vel.x - 16384, Int2Fx(-7));
            FLAG_ON(hero, FLG_X_FLIP);
        }
    }

    const Bool overlapping = Rcollide(Radd(hero->box, hero->pos), Radd(actor->box, actor->pos));
    if (overlapping && actor->pos.y >= Int2Fx(300)) {
        if (!ANY_FLAG(actor, FLG_CAGE_BROKEN))
            hero->vel.x = Fx0;

        ++VAL(actor, CAGE_STATE);
    }

    if (VAL(actor, CAGE_STATE) > 100) {
        if (!ANY_FLAG(actor, FLG_CAGE_BROKEN)) {
            if (TOUCHING(hero, TOUCH_BOTTOM)) {
                hero->vel.y = Fmul(Int2Fx(-13), get_player_jump(player));

                play_state_sound("jump", PLAY_POS, A_ACTOR(hero));
            }

            if (overlapping && hero->pos.y < (actor->pos.y - Int2Fx(50))) {
                FLAG_ON(actor, FLG_CAGE_BROKEN);

                create_cage_shard(Vadd(actor->pos, (FVec2){Int2Fx(-24), Int2Fx(-48)}));
                create_cage_shard(Vadd(actor->pos, (FVec2){Int2Fx(-9), Int2Fx(-33)}));
                create_cage_shard(Vadd(actor->pos, (FVec2){Int2Fx(9), Int2Fx(-60)}));
                create_cage_shard(Vadd(actor->pos, (FVec2){Int2Fx(-8), Int2Fx(-70)}));
                create_cage_shard(Vadd(actor->pos, (FVec2){Int2Fx(-24), Int2Fx(-66)}));

                play_state_sound("break", PLAY_POS, A_ACTOR(actor));
            }
        }

        if (ANY_FLAG(actor, FLG_CAGE_BROKEN) && TOUCHING(hero, TOUCH_BOTTOM)) {
            sequence->state = 1;

            for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
                const GamePlayer* oplayer = get_player(i);
                if (oplayer == NULL)
                    continue;

                GameActor* pawn = get_actor(oplayer->actor);
                if (pawn != NULL && pawn->type == ACT_PLAYER) {
                    if (pawn->vel.x < Int2Fx(7))
                        pawn->vel.x = Fmin(pawn->vel.x + 19115, Int2Fx(7));
                    FLAG_OFF(pawn, FLG_X_FLIP);
                }
            }
        }
    }
}

static void draw_cage(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, ANY_FLAG(actor, FLG_CAGE_BROKEN) ? "markers/cage/broken" : "markers/cage", FALSE);

    const Sint32 top = Fx2Int(actor->box.start.y);
    for (Sint32 i = (Fx2Int(actor->pos.y + actor->box.start.y) / 32) * 32; i >= 0; i -= 32) {
        batch_offset(B_F3_XY(6.f, -top + i + 32.f));
        batch_sprite("markers/cage/chain");
    }
}

const ActorTable TAB_PEACH_CAGE = {
    .load = load_cage,
    .create = create_cage,
    .tick = tick_cage,
    .draw = draw_cage,
};

/* ==========
   CAGE SHARD
   ========== */

static void load_cage_shard() {
    load_sprite("markers/cage/shard", AKL_NEVER);
}

static void tick_cage_shard(GameActor* actor) {
    VAL(actor, CAGE_ANGLE) = Fmod(VAL(actor, CAGE_ANGLE) + 25736, Fx2Pi);

    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 13107;

    if (!in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_cage_shard(const GameActor* actor) {
    batch_reset();
    batch_angle(Fx2Float(VAL(actor, CAGE_ANGLE)));
    draw_actor(actor, "markers/cage/shard", FALSE);
}

const ActorTable TAB_PEACH_CAGE_SHARD = {
    .load = load_cage_shard,
    .tick = tick_cage_shard,
    .draw = draw_cage_shard,
};
