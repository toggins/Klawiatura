#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_player.h"
#include "actors/K_projectiles.h"

typedef Uint8 BooAnimations;
enum {
    BOOA_IDLE,
    BOOA_BLINK,
    BOOA_ACTIVE,
};

enum {
    VAL_BOO_ANIMATION,
    VAL_BOO_FRAME,

    VAL_EERIE_FRAME = 0,
    VAL_EERIE_SPAWN_X,
    VAL_EERIE_SPAWN_Y,

    VAL_GHOUL_HEALTH = 0,
    VAL_GHOUL_SPEED,
    VAL_GHOUL_ATTACK_CHANCE,
    VAL_GHOUL_ATTACK_SPEED,
    VAL_GHOUL_BOOS,
    VAL_GHOUL_BOOS_TIME,
    VAL_GHOUL_HURT_DURATION,
    VAL_GHOUL_START_X,
    VAL_GHOUL_END_X,

    VAL_GHOUL_ATTACK,
    VAL_GHOUL_Y,
    VAL_GHOUL_ANGLE,
    VAL_GHOUL_ATTACK_ANGLE,
    VAL_GHOUL_HURT,
    VAL_GHOUL_HITS,
    VAL_GHOUL_FADE,

    VAL_GHOUL_DEAD = 0,
    VAL_GHOUL_DEAD_FADE,
};

#define FLG_GHOUL_ACTIVE CUSTOM_FLAG(0)
#define FLG_GHOUL_FADE_IN CUSTOM_FLAG(1)

/* ===
   BOO
   === */

static void load_boo() {
    load_sprite_num("enemies/boo/%u", 2, AKL_NEVER);
    load_sprite_num("enemies/boo/blink/%u", 10, AKL_NEVER);
    load_sprite("enemies/boo/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_boo(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(2);
}

static void tick_boo(GameActor* actor) {
    if (VAL(actor, BOO_ANIMATION) == BOOA_BLINK) {
        if (++VAL(actor, BOO_FRAME) >= 10) {
            VAL(actor, BOO_ANIMATION) = BOOA_IDLE;
            VAL(actor, BOO_FRAME) = 0;
        }
    }

    Bool active = FALSE;

    Fixed angle = Fx0, score = FxUpper;
    const GameActor* nearest = NULL;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (!in_player_view(player, actor->pos, Int2Fx(-64), VEF_ALL))
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER)
            continue;

        const FVec2 ppos = Vadd(pawn->pos, (FVec2){Fx1, Int2Fx(-15)});
        const Fixed dist = Vdist(actor->pos, ppos);
        if (nearest == NULL || dist < score) {
            nearest = pawn;
            score = dist;
            angle = Fmul(Ffloor(Fdiv(Vtheta(actor->pos, ppos), 12868) + FxHalf), 12868);
        }
    }

    if (nearest != NULL) {
        if (actor->pos.x > nearest->pos.x)
            FLAG_ON(actor, FLG_X_FLIP);
        else if (actor->pos.x < nearest->pos.x)
            FLAG_OFF(actor, FLG_X_FLIP);

        if ((ANY_FLAG(actor, FLG_X_FLIP) && ANY_FLAG(nearest, FLG_X_FLIP))
            || (!ANY_FLAG(actor, FLG_X_FLIP) && !ANY_FLAG(nearest, FLG_X_FLIP)))
        {
            active = TRUE;
        }
    }

    if (active) {
        move_actor(actor, Vadd(actor->pos, (FVec2){Fmul(73728, Fcos(angle)), Fmul(73728, -Fsin(angle))}));

        VAL(actor, BOO_ANIMATION) = BOOA_ACTIVE;
        VAL(actor, BOO_FRAME) = 0;
    } else {
        if (VAL(actor, BOO_ANIMATION) == BOOA_ACTIVE) {
            VAL(actor, BOO_ANIMATION) = BOOA_IDLE;
            VAL(actor, BOO_FRAME) = 0;
        }

        if (VAL(actor, BOO_ANIMATION) == BOOA_IDLE && (gamestate()->time % 5) == 0 && rng(20) == 10) {
            VAL(actor, BOO_ANIMATION) = BOOA_BLINK;
            VAL(actor, BOO_FRAME) = 0;
        }
    }
}

static void draw_boo(const GameActor* actor) {
    batch_reset();

    const char* sprite = "enemies/boo/0";
    switch (VAL(actor, BOO_ANIMATION)) {
    default:
        break;
    case BOOA_BLINK:
        sprite = fmt("enemies/boo/blink/%i", VAL(actor, BOO_FRAME) % 10);
        break;
    case BOOA_ACTIVE:
        sprite = "enemies/boo/1";
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_dead_boo(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/boo/dead", FALSE);
}

static void collide_boo(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;
    case ACT_PLAYER:
        maybe_hit_player(actor, from);
        break;
    case ACT_HAMMER_PROJECTILE:
        hit_hammer(actor, from, 200);
        break;
    }
}

const ActorTable TAB_BOO = {
    .load = load_boo,
    .create = create_boo,
    .tick = tick_boo,
    .draw = draw_boo,
    .draw_dead = draw_dead_boo,
    .collide = collide_boo,
};

/* =====
   EERIE
   ===== */

static void load_eerie() {
    load_sprite_num("enemies/eerie/%u", 2, AKL_NEVER);
    load_sprite("enemies/eerie/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_eerie(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(2);

    VAL(actor, EERIE_SPAWN_X) = actor->pos.x;
    VAL(actor, EERIE_SPAWN_Y) = actor->pos.y;
}

static void tick_eerie(GameActor* actor) {
    VAL(actor, EERIE_FRAME) += 3;

    if (in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        move_actor(actor, Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-3) : Int2Fx(3), Fx0}));

    if (gamecontext()->num_players > 1 && !in_any_view(actor->pos, Int2Fx(-64), VEF_ALL)) {
        const FVec2 spawn = (FVec2){VAL(actor, EERIE_SPAWN_X), VAL(actor, EERIE_SPAWN_Y)};
        if (!in_any_view(spawn, Int2Fx(-64), VEF_ALL)) {
            move_actor(actor, spawn);

            skip_interp(actor);
        }
    }
}

static void draw_eerie(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/eerie/%i", (VAL(actor, EERIE_FRAME) / 25) % 2), FALSE);
}

static void draw_dead_eerie(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/eerie/dead", FALSE);
}

const ActorTable TAB_EERIE = {
    .load = load_eerie,
    .create = create_eerie,
    .tick = tick_eerie,
    .draw = draw_eerie,
    .draw_dead = draw_dead_eerie,
    .collide = collide_boo,
};

/* =======
   BIG BOO
   ======= */

static void load_big_boo() {
    load_sprite_num("enemies/boo/big/%u", 2, AKL_NEVER);
    load_sprite("ui/boss/big_boo", AKL_NEVER);
    load_sprite("ui/boss/bar", AKL_NEVER);
    load_sprite("ui/boss/bar2", AKL_NEVER);
    load_sound("bowser/hurt", AKL_NEVER);
    load_sound("bowser/dead", AKL_NEVER);
    load_actor(ACT_BIG_BOO_DEAD);
}

static void create_big_boo(GameActor* actor) {
    actor->box.start.x = actor->box.start.y = Int2Fx(-63);
    actor->box.end.x = Int2Fx(64);
    actor->box.end.y = Int2Fx(65);

    actor->depth = 2;

    VAL(actor, GHOUL_HEALTH) = 4;
    VAL(actor, GHOUL_SPEED) = Int2Fx(2);
    VAL(actor, GHOUL_ATTACK_CHANCE) = 1;
    VAL(actor, GHOUL_ATTACK_SPEED) = Fx1;
    VAL(actor, GHOUL_BOOS) = 4;
    VAL(actor, GHOUL_BOOS_TIME) = 333;
    VAL(actor, GHOUL_HURT_DURATION) = 100;

    VAL(actor, GHOUL_Y) = actor->pos.y;
}

static void tick_big_boo(GameActor* actor) {
    VAL(actor, GHOUL_ANGLE) += 3431;

    if (!ANY_FLAG(actor, FLG_GHOUL_ACTIVE) && in_any_view(actor->pos, Int2Fx(-64), VEF_ALL)) {
        FLAG_ON(actor, FLG_GHOUL_ACTIVE);

        // !!! CLIENT-SIDE !!!
        VideoBoss* vboss = &videostate()->boss;
        vboss->active = TRUE;
        vboss->type = VBT_BIG_BOO;
        // !!! CLIENT-SIDE !!!
    }

    if (ANY_FLAG(actor, FLG_GHOUL_ACTIVE)) {
        const Fixed last_attack = VAL(actor, GHOUL_ATTACK);
        VAL(actor, GHOUL_ATTACK) += VAL(actor, GHOUL_ATTACK_CHANCE);

        if (last_attack <= 300 && VAL(actor, GHOUL_ATTACK) > 300) {
            const FVec2 npos = nearest_player_pos(actor->pos);
            if (npos.x < actor->pos.x)
                FLAG_ON(actor, FLG_X_FLIP);
            if (npos.x > actor->pos.x)
                FLAG_OFF(actor, FLG_X_FLIP);
        }
        if (VAL(actor, GHOUL_ATTACK) > 350) {
            VAL(actor, GHOUL_ATTACK_ANGLE) += 1144;
            if (VAL(actor, GHOUL_ATTACK_ANGLE) >= FxPi) {
                VAL(actor, GHOUL_ATTACK) = 0;
                VAL(actor, GHOUL_ATTACK_ANGLE) = Fx0;
            }
        }

        if (VAL(actor, GHOUL_ATTACK) < 300 || VAL(actor, GHOUL_ATTACK) > 350) {
            const Fixed spd = Fmul(VAL(actor, GHOUL_SPEED), (VAL(actor, GHOUL_ATTACK) > 350) ? Int2Fx(2) : Fx1);
            actor->vel.x = ANY_FLAG(actor, FLG_X_FLIP) ? -spd : spd;
        } else {
            actor->vel.x = Fx0;
        }

        if (VAL(actor, GHOUL_START_X) != VAL(actor, GHOUL_END_X)) {
            if ((actor->pos.x + actor->box.start.x) < VAL(actor, GHOUL_START_X))
                FLAG_OFF(actor, FLG_X_FLIP);
            if ((actor->pos.x + actor->box.end.x) > VAL(actor, GHOUL_END_X))
                FLAG_ON(actor, FLG_X_FLIP);
        }

        if (VAL(actor, GHOUL_BOOS) > 0
            && (VAL(actor, GHOUL_BOOS_TIME) <= 1 || (gamestate()->time % VAL(actor, GHOUL_BOOS_TIME)) == 0)
            && VAL(actor, GHOUL_ATTACK) < 300)
        {
            create_actor(ACT_BOO, actor->pos);
            --VAL(actor, GHOUL_BOOS);
        }
    }

    move_actor(actor, Vadd((FVec2){actor->pos.x, VAL(actor, GHOUL_Y) - Fmul(Int2Fx(16), Fsin(VAL(actor, GHOUL_ANGLE)))
                                                     + Fmul(Int2Fx(250), Fsin(VAL(actor, GHOUL_ATTACK_ANGLE)))},
                          actor->vel));

    if (VAL(actor, GHOUL_HITS) < -4) {
        --VAL(actor, GHOUL_HEALTH);
        VAL(actor, GHOUL_HURT) = VAL(actor, GHOUL_HURT_DURATION);
        VAL(actor, GHOUL_HITS) = 0;

        play_state_sound("bowser/hurt", PLAY_POS, A_ACTOR(actor));
    }

    if (VAL(actor, GHOUL_HURT) > 0) {
        --VAL(actor, GHOUL_HURT);

        if (VAL(actor, GHOUL_FADE) < 100 && !ANY_FLAG(actor, FLG_GHOUL_FADE_IN))
            VAL(actor, GHOUL_FADE) += 5;
        if (VAL(actor, GHOUL_FADE) >= 100)
            FLAG_ON(actor, FLG_GHOUL_FADE_IN);
    }

    if (VAL(actor, GHOUL_FADE) > 0 && (ANY_FLAG(actor, FLG_GHOUL_FADE_IN) || VAL(actor, GHOUL_HURT) <= 0))
        VAL(actor, GHOUL_FADE) -= 5;

    if (VAL(actor, GHOUL_HURT) > 0 && VAL(actor, GHOUL_FADE) <= 0)
        FLAG_OFF(actor, FLG_GHOUL_FADE_IN);

    if (VAL(actor, GHOUL_HEALTH) <= 0) {
        if (get_num_actors(ACT_BIG_BOO) <= 1 && get_num_actors(ACT_BOWSER) <= 0) {
            set_sequence(GS_BOSS_END, get_player(actor->player), 0);

            GameState* game_state = gamestate();
            if (game_state->flags & GF_LOST_MAP) {
                game_state->flags |= GF_LOST_MAP_END;
                play_state_track(ALL_TRACKS, "smb3/boss_clear", 0, 0);
            }
        }

        GameActor* dead = create_actor(ACT_BIG_BOO_DEAD, actor->pos);
        if (dead != NULL) {
            FLAG_ON(dead, actor->flags & FLG_X_FLIP);
            align_interp(dead, actor);
        }
        FLAG_ON(actor, FLG_DESTROY);

        play_state_sound("bowser/dead", PLAY_POS, A_ACTOR(actor));
        return;
    }
}

static void draw_big_boo(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA((1.f - ((float)VAL(actor, GHOUL_FADE) / 128.f)) * 255.f));
    draw_actor(actor, fmt("enemies/boo/big/%i", VAL(actor, GHOUL_ATTACK) > 300), FALSE);
}

static void draw_big_boo_hud(const GameActor* actor) {
    videostate()->boss.health += VAL(actor, GHOUL_HEALTH);
}

static void collide_big_boo(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (VAL(from, PLAYER_STARMAN) > 0 || (gamecontext()->num_players > 1 && VAL(actor, GHOUL_ATTACK) < 300))
            break;

        if (from->pos.y < (actor->pos.y - Int2Fx(45))) {
            if (VAL(actor, GHOUL_HURT) > 0 || (from->vel.y < Fx0 && !ANY_FLAG(from, FLG_PLAYER_STOMP)))
                break;

            actor->player = from->player;
            --VAL(actor, GHOUL_HEALTH);
            VAL(actor, GHOUL_HURT) = VAL(actor, GHOUL_HURT_DURATION);
            VAL(actor, GHOUL_HITS) = 0;

            GamePlayer* player = get_player(from->player);
            if (player != NULL)
                player->score += 100;
            from->vel.y = Fmul(Int2Fx(-8), get_player_jump(player));
            VAL(from, PLAYER_SLIP) = ANY_FLAG(from, FLG_X_FLIP) ? -8 : 8;

            play_state_sound("bowser/hurt", PLAY_POS, A_ACTOR(actor));
        } else {
            hit_player(from);
        }

        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        if (get_player(from->player) == NULL)
            break;

        if (VAL(from, PROJECTILE_COOLDOWN) > 0) {
            VAL(from, PROJECTILE_COOLDOWN) = 2;
            break;
        }

        actor->player = from->player;
        --VAL(actor, GHOUL_HITS);

        VAL(from, PROJECTILE_COOLDOWN) = 2;
        FLAG_ON(from, FLG_PROJECTILE_HIT | FLG_PROJECTILE_HIT_BOSS);
        create_actor(ACT_EXPLODE, from->pos);

        play_state_sound("kick", PLAY_POS, A_ACTOR(from));
        break;
    }
    }
}

const ActorTable TAB_BIG_BOO = {
    .load = load_big_boo,
    .create = create_big_boo,
    .tick = tick_big_boo,
    .draw = draw_big_boo,
    .draw_hud = draw_big_boo_hud,
    .collide = collide_big_boo,
};

/* =============
   BIG BOO, DEAD
   ============= */

static void load_big_boo_dead() {
    load_sprite("enemies/boo/big/0", AKL_NEVER);
    load_sound("transform", AKL_NEVER);
}

static void create_big_boo_dead(GameActor* actor) {
    actor->depth = 2;
}

static void tick_big_boo_dead(GameActor* actor) {
    ++VAL(actor, GHOUL_DEAD);
    if (VAL(actor, GHOUL_DEAD) == 101)
        play_state_sound("transform", PLAY_POS, A_ACTOR(actor));

    if (VAL(actor, GHOUL_DEAD) > 100) {
        actor->vel.y += 2621;
        if (VAL(actor, GHOUL_DEAD_FADE) < 130)
            VAL(actor, GHOUL_DEAD_FADE) += 2;
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (below_nearest_bounds(actor->pos, Int2Fx(100)))
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_big_boo_dead(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA((1.f - ((float)SDL_min(VAL(actor, GHOUL_DEAD_FADE), 128) / 128.f)) * 255.f));
    draw_actor(actor, "enemies/boo/big/0", FALSE);
}

const ActorTable TAB_BIG_BOO_DEAD = {
    .load = load_big_boo_dead,
    .create = create_big_boo_dead,
    .tick = tick_big_boo_dead,
    .draw = draw_big_boo_dead,
};
