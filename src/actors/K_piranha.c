#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_enemies.h"
#include "actors/K_player.h"
#include "actors/K_projectiles.h"

enum {
    VAL_PIRANHA_MOVE,
    VAL_PIRANHA_WAIT,
    VAL_PIRANHA_FIRE,
    VAL_PIRANHA_FRAME,
};

#define FLG_PIRANHA_BLOCKED CUSTOM_FLAG(0)
#define FLG_PIRANHA_MOVE CUSTOM_FLAG(1)
#define FLG_PIRANHA_HIDE CUSTOM_FLAG(2)
#define FLG_PIRANHA_FIRE CUSTOM_FLAG(3)
#define FLG_PIRANHA_SHOULD_FIRE CUSTOM_FLAG(4)
#define FLG_PIRANHA_RED CUSTOM_FLAG(5)

/* =============
   PIRANHA PLANT
   ============= */

static void load() {
    load_sprite_num("enemies/piranha/%u", 2, AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_PIRANHA_FIRE)) {
        load_sprite_num(
            ANY_FLAG(actor, FLG_PIRANHA_RED) ? "enemies/piranha/red/fire/%u" : "enemies/piranha/fire/%u", 2, AKL_NEVER);
        load_sound("fire", AKL_NEVER);
        load_actor(ACT_FIREBALL_PROJECTILE);
    } else if (ANY_FLAG(actor, FLG_PIRANHA_RED)) {
        load_sprite_num("enemies/piranha/red/%u", 2, AKL_NEVER);
    }
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-46);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(21);

    FLAG_ON(actor, FLG_PIRANHA_SHOULD_FIRE);
}

static void tick(GameActor* actor) {
    VAL(actor, PIRANHA_FRAME) += ANY_FLAG(actor, FLG_PIRANHA_FIRE) ? 24 : 14;

    // EVENTS FROM "Level 6 - 1"

    // 66
    const GameState* game_state = gamestate();
    if ((game_state->time % 10) == 0)
        FLAG_OFF(actor, FLG_PIRANHA_BLOCKED);

    // 662
    if (game_state->time == 0) {
        if (ANY_FLAG(actor, FLG_Y_FLIP)) {
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(-60)}));
            actor->box = Ryflip(actor->box);
        } else {
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(60)}));
        }
        skip_interp(actor);
    }

    // 665
    const FVec2 ppos = nearest_player_pos(actor->pos);
    const Fixed range = ANY_FLAG(actor, FLG_PIRANHA_RED) ? Int2Fx(40) : Int2Fx(80);
    if (actor->pos.x < (ppos.x + range) && actor->pos.x > (ppos.x - range))
        FLAG_ON(actor, FLG_PIRANHA_BLOCKED);

    // 666
    if (!ANY_FLAG(actor, FLG_PIRANHA_BLOCKED | FLG_PIRANHA_MOVE | FLG_PIRANHA_HIDE)
        && in_any_view(actor->pos, Int2Fx(-128), VEF_ALL))
    {
        FLAG_ON(actor, FLG_PIRANHA_MOVE);
        VAL(actor, PIRANHA_MOVE) = ANY_FLAG(actor, FLG_Y_FLIP) ? 60 : -60;
    }

    // 667
    if (VAL(actor, PIRANHA_MOVE) < 0 && ANY_FLAG(actor, FLG_PIRANHA_MOVE)) {
        const ActorValue move = ANY_FLAG(actor, FLG_PIRANHA_RED) ? 2 : 1;
        move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(ANY_FLAG(actor, FLG_Y_FLIP) ? move : -move)}));
        VAL(actor, PIRANHA_MOVE) += move;
    }

    // 668
    if (VAL(actor, PIRANHA_MOVE) == 0 && ANY_FLAG(actor, FLG_PIRANHA_MOVE) && !ANY_FLAG(actor, FLG_PIRANHA_HIDE))
        ++VAL(actor, PIRANHA_WAIT);

    // 669
    if (ANY_FLAG(actor, FLG_PIRANHA_MOVE) && !ANY_FLAG(actor, FLG_PIRANHA_HIDE) && VAL(actor, PIRANHA_WAIT) > 50) {
        VAL(actor, PIRANHA_WAIT) = 0;
        VAL(actor, PIRANHA_MOVE) = ANY_FLAG(actor, FLG_Y_FLIP) ? -60 : 60;
        FLAG_ON(actor, FLG_PIRANHA_HIDE | FLG_PIRANHA_SHOULD_FIRE);
    }

    // 670
    if (VAL(actor, PIRANHA_MOVE) == 0 && ALL_FLAG(actor, FLG_PIRANHA_MOVE | FLG_PIRANHA_FIRE | FLG_PIRANHA_SHOULD_FIRE)
        && !ANY_FLAG(actor, FLG_PIRANHA_HIDE))
    {
        VAL(actor, PIRANHA_FIRE) = (game_state->flags & GF_FUNNY_TANKS) ? 30 : 3;
        FLAG_OFF(actor, FLG_PIRANHA_SHOULD_FIRE);
    }

    // 671
    if (VAL(actor, PIRANHA_MOVE) > 0 && ANY_FLAG(actor, FLG_PIRANHA_MOVE)) {
        const ActorValue move = ANY_FLAG(actor, FLG_PIRANHA_RED) ? 2 : 1;
        move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(ANY_FLAG(actor, FLG_Y_FLIP) ? -move : move)}));
        VAL(actor, PIRANHA_MOVE) -= move;
    }

    // 672, 673
    if (ALL_FLAG(actor, FLG_PIRANHA_MOVE | FLG_PIRANHA_HIDE)) {
        if (VAL(actor, PIRANHA_MOVE) == 0)
            ++VAL(actor, PIRANHA_WAIT);

        if (VAL(actor, PIRANHA_WAIT) > 50) {
            VAL(actor, PIRANHA_WAIT) = 0;
            FLAG_OFF(actor, FLG_PIRANHA_MOVE | FLG_PIRANHA_HIDE);
        }
    }

    // 674
    if (VAL(actor, PIRANHA_FIRE) > 0 && (game_state->time % ((game_state->flags & GF_FUNNY_TANKS) ? 2 : 10)) == 0) {
        GameActor* fireball = create_actor(ACT_FIREBALL_PROJECTILE,
            Vadd(actor->pos, (FVec2){Fx0, (ANY_FLAG(actor, FLG_Y_FLIP) ? Int2Fx(27) : Int2Fx(-26))}));
        if (fireball != NULL) {
            fireball->vel.x = Int2Fx(rng(5));
            fireball->vel.x -= Int2Fx(rng(5));
            fireball->vel.y = ANY_FLAG(actor, FLG_Y_FLIP) ? Int2Fx(rng(6)) : (Int2Fx(-3) - Int2Fx(rng(9)));
            FLAG_ON(fireball, FLG_PROJECTILE_ALT);
        }

        --VAL(actor, PIRANHA_FIRE);
        play_state_sound("fire", PLAY_POS, A_ACTOR(actor));
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_PIRANHA_FIRE)
                ? (ANY_FLAG(actor, FLG_PIRANHA_RED) ? "enemies/piranha/red/fire/%i" : "enemies/piranha/fire/%i")
                : (ANY_FLAG(actor, FLG_PIRANHA_RED) ? "enemies/piranha/red/%i" : "enemies/piranha/%i"),
            (VAL(actor, PIRANHA_FRAME) / 100) % 2),
        FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (VAL(actor, PLAYER_STARMAN) > 0) {
            player_starman(from, actor);
        } else if (ANY_FLAG(actor, FLG_Y_FLIP)) {
            GamePlayer* player = get_player(from->player);
            if (from->pos.y
                < (actor->pos.y + ((player == NULL || player->powerup == POW_NONE) ? Int2Fx(58) : Int2Fx(84))))
            {
                maybe_hit_player(actor, from);
            }
        } else if (from->pos.y > (actor->pos.y - Int2Fx(40))) {
            maybe_hit_player(actor, from);
        }

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
        create_actor(
            ACT_EXPLODE, Vadd(actor->pos, (FVec2){Fx0, ANY_FLAG(actor, FLG_Y_FLIP) ? Int2Fx(32) : Int2Fx(-24)}));
        break;
    }
    }
}

const ActorTable TAB_PIRANHA_PLANT = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* ============
   PIRANHA HEAD
   ============ */

static void load_head() {
    load_sprite_num("enemies/piranha/head/%u", 4, AKL_NEVER);
}

static void load_head_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_PIRANHA_FIRE)) {
        load_sprite_num("enemies/piranha/head/fire/%u", 4, AKL_NEVER);
        load_sound("fire", AKL_NEVER);
        load_actor(ACT_FIREBALL_PROJECTILE);
    }
}

static void create_head(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-30);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Int2Fx(3);
}

static void tick_head(GameActor* actor) {
    VAL(actor, PIRANHA_FRAME) += ANY_FLAG(actor, FLG_PIRANHA_FIRE) ? 15 : 11;

    const GameState* game_state = gamestate();
    if (game_state->time == 0 && ANY_FLAG(actor, FLG_Y_FLIP))
        actor->box = Ryflip(actor->box);

    if (!ANY_FLAG(actor, FLG_PIRANHA_FIRE))
        return;

    if (in_any_view(actor->pos, Int2Fx(-128), VEF_ALL))
        --VAL(actor, PIRANHA_WAIT);

    if (VAL(actor, PIRANHA_WAIT) <= -50) {
        VAL(actor, PIRANHA_FIRE) = (game_state->flags & GF_FUNNY_TANKS) ? 30 : 3;
        VAL(actor, PIRANHA_WAIT) = 150;
    }

    if (VAL(actor, PIRANHA_FIRE) > 0 && (game_state->time % ((game_state->flags & GF_FUNNY_TANKS) ? 2 : 10)) == 0) {
        GameActor* fireball = create_actor(ACT_FIREBALL_PROJECTILE,
            Vadd(actor->pos, (FVec2){Fx0, (ANY_FLAG(actor, FLG_Y_FLIP) ? Int2Fx(12) : Int2Fx(-12))}));
        if (fireball != NULL) {
            fireball->vel.x = Int2Fx(rng(5));
            fireball->vel.x -= Int2Fx(rng(5));
            fireball->vel.y = ANY_FLAG(actor, FLG_Y_FLIP) ? Int2Fx(rng(6)) : (Int2Fx(-3) - Int2Fx(rng(9)));
            FLAG_ON(fireball, FLG_PROJECTILE_ALT);
        }

        --VAL(actor, PIRANHA_FIRE);
        play_state_sound("fire", PLAY_POS, A_ACTOR(actor));
    }
}

static void draw_head(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt(ANY_FLAG(actor, FLG_PIRANHA_FIRE) ? "enemies/piranha/head/fire/%i" : "enemies/piranha/head/%i",
            (VAL(actor, PIRANHA_FRAME) / 50) % 4),
        FALSE);
}

static void collide_head(GameActor* actor, GameActor* from) {
    (void)actor;

    hit_player(from);
}

const ActorTable TAB_PIRANHA_HEAD = {
    .load = load_head,
    .load_special = load_head_special,
    .create = create_head,
    .tick = tick_head,
    .draw = draw_head,
    .collide = collide_head,
};
