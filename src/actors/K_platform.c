#include "K_string.h"
#include "K_video.h"

#include "actors/K_checkpoint.h"
#include "actors/K_platform.h"

/* ========
   PLATFORM
   ======== */

static SolidFlags is_solid(const GameActor* actor) {
    switch (VAL(actor, PLATFORM_TYPE)) {
    default:
        break;
    case PLAT_GRASS:
    case PLAT_GRASS_SMALL:
    case PLAT_BRICK_BIG:
    case PLAT_BRICK_TALL:
    case PLAT_BRICK_SMALL:
        return SOL_SOLID;
    }

    return SOL_TOP;
}

static void load_special(const GameActor* actor) {
    switch (VAL(actor, PLATFORM_TYPE)) {
    default:
        load_sprite("markers/platform/normal", AKL_NEVER);
        break;
    case PLAT_SMALL:
        load_sprite("markers/platform/small", AKL_NEVER);
        break;
    case PLAT_CLOUD:
        load_sprite_num("markers/platform/cloud/%u", 4, AKL_NEVER);
        break;
    case PLAT_PURPLE:
        load_sprite("markers/platform/purple", AKL_NEVER);
        break;
    case PLAT_BRICK:
        load_sprite("markers/platform/brick/normal", AKL_NEVER);
        break;
    case PLAT_GRASS:
        load_sprite("markers/platform/grass/normal", AKL_NEVER);
        break;
    case PLAT_GRASS_SMALL:
        load_sprite("markers/platform/grass/small", AKL_NEVER);
        break;
    case PLAT_BRICK_BIG:
        load_sprite("markers/platform/brick/big", AKL_NEVER);
        break;
    case PLAT_BRICK_TALL:
        load_sprite("markers/platform/brick/tall", AKL_NEVER);
        break;
    case PLAT_BRICK_SMALL:
        load_sprite("markers/platform/brick/small", AKL_NEVER);
        break;
    case PLAT_BRICK_BUTTONS:
        load_sprite("markers/platform/brick/buttons", AKL_NEVER);
        break;
    case PLAT_BLOCK:
        load_sprite("markers/platform/block", AKL_NEVER);
        break;
    }
}

static void create(GameActor* actor) {
    actor->depth = Int2Fx(19);
}

static void pre_tick(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (game_state->time == 0) {
        switch (VAL(actor, PLATFORM_TYPE)) {
        default: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(95);
            actor->box.end.y = Int2Fx(16);
            break;
        }

        case PLAT_SMALL: {
            actor->box.start.x = Int2Fx(30);
            actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(60);
            actor->box.end.y = Int2Fx(16);
            break;
        }

        case PLAT_CLOUD: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(127);
            actor->box.end.y = Int2Fx(32);
            break;
        }

        case PLAT_PURPLE: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(96);
            actor->box.end.y = Int2Fx(18);
            break;
        }

        case PLAT_BRICK: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(64);
            actor->box.end.y = Int2Fx(32);
            break;
        }

        case PLAT_GRASS: {
            actor->box.start.x = Int2Fx(-94);
            actor->box.start.y = Int2Fx(-190);
            actor->box.end.x = Int2Fx(98);
            actor->box.end.y = Int2Fx(2);
            break;
        }

        case PLAT_GRASS_SMALL: {
            actor->box.start.x = Int2Fx(-31);
            actor->box.start.y = Int2Fx(-191);
            actor->box.end.x = Int2Fx(33);
            actor->box.end.y = Fx1;
            break;
        }

        case PLAT_BRICK_BIG: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(120);
            actor->box.end.y = Int2Fx(32);
            break;
        }

        case PLAT_BRICK_TALL: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(32);
            actor->box.end.y = Int2Fx(64);
            break;
        }

        case PLAT_BRICK_SMALL:
        case PLAT_BLOCK: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = actor->box.end.y = Int2Fx(32);
            break;
        }

        case PLAT_BRICK_BUTTONS: {
            actor->box.start.x = actor->box.start.y = Fx0;
            actor->box.end.x = Int2Fx(76);
            actor->box.end.y = Int2Fx(32);
            break;
        }
        }

        if (ANY_FLAG(actor, FLG_PLATFORM_RUN)) {
            const GameActor* checkpoint = get_actor(game_state->checkpoint);
            if (checkpoint != NULL && ANY_FLAG(checkpoint, FLG_CHECKPOINT_SET_PLATFORM)) {
                move_actor(
                    actor, (FVec2){VAL(checkpoint, CHECKPOINT_PLATFORM_X), VAL(checkpoint, CHECKPOINT_PLATFORM_Y)});
                actor->last_pos = actor->pos;
                skip_interp(actor);
            }
        }

        VAL(actor, PLATFORM_START_X) = actor->pos.x;
        VAL(actor, PLATFORM_START_Y) = actor->pos.y;
        VAL(actor, PLATFORM_START_X_SPEED) = actor->vel.x;
        VAL(actor, PLATFORM_START_Y_SPEED) = actor->vel.y;
        VAL(actor, PLATFORM_START_FLAGS) = (ActorValue)actor->flags;
    }

    if (ANY_FLAG(actor, FLG_PLATFORM_FALLING) && actor->vel.y < Int2Fx(10))
        actor->vel.y = Fmin(actor->vel.y + 13107, Int2Fx(10));

    move_actor(actor, Vadd(actor->pos, actor->vel));
    collide_actor(actor);
    push_actors(actor);

    if (ANY_FLAG(actor, FLG_PLATFORM_WRAP)) {
        const Fixed lh = levelinfo()->size.y;

        if (actor->vel.y > Fx0 && actor->pos.y > (lh + Int2Fx(10))) {
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, -lh - Int2Fx(20)}));
            actor->last_pos = actor->pos;
            push_actors(actor);
            skip_interp(actor);
        }

        if (actor->vel.y < Fx0 && actor->pos.y < -Int2Fx(10)) {
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, lh + Int2Fx(20)}));
            actor->last_pos = actor->pos;
            push_actors(actor);
            skip_interp(actor);
        }
    }

#define DO_RESPAWN()                                                                                                   \
    do {                                                                                                               \
        move_actor(actor, (FVec2){VAL(actor, PLATFORM_START_X), VAL(actor, PLATFORM_START_Y)});                        \
        actor->last_pos = actor->pos;                                                                                  \
        actor->vel.x = VAL(actor, PLATFORM_START_X_SPEED);                                                             \
        actor->vel.y = VAL(actor, PLATFORM_START_Y_SPEED);                                                             \
        actor->flags = VAL(actor, PLATFORM_START_FLAGS);                                                               \
                                                                                                                       \
        VAL(actor, PLATFORM_RESPAWN) = 0;                                                                              \
        skip_interp(actor);                                                                                            \
    } while (FALSE);

    if (ANY_FLAG(actor, FLG_PLATFORM_FALLING)
        && (actor->pos.y + actor->box.start.y) > (levelinfo()->size.y + Int2Fx(64)))
    {
        if (gamecontext()->num_players <= 1)
            FLAG_ON(actor, FLG_DESTROY);
        else if (++VAL(actor, PLATFORM_RESPAWN) > 150)
            DO_RESPAWN();
    }

    if (ANY_FLAG(actor, FLG_PLATFORM_RUNNING) && gamecontext()->num_players > 1) {
        GameActor* checkpoint = get_actor(game_state->checkpoint);
        if (checkpoint != NULL && ANY_FLAG(checkpoint, FLG_CHECKPOINT_SET_PLATFORM)) {
            VAL(actor, PLATFORM_START_X) = VAL(checkpoint, CHECKPOINT_PLATFORM_X);
            VAL(actor, PLATFORM_START_Y) = VAL(checkpoint, CHECKPOINT_PLATFORM_Y);
        }

        if (++VAL(actor, PLATFORM_RESPAWN) > 650)
            DO_RESPAWN();
    }

#undef DO_RESPAWN
}

static void draw(const GameActor* actor) {
    batch_reset();

    const char* sprite = NULL;
    switch (VAL(actor, PLATFORM_TYPE)) {
    default:
        sprite = "markers/platform/normal";
        break;
    case PLAT_SMALL:
        sprite = "markers/platform/small";
        break;
    case PLAT_CLOUD:
        sprite = fmt("markers/platform/cloud/%i", ((gamestate()->time * 2) / 25) % 4);
        break;
    case PLAT_PURPLE:
        sprite = "markers/platform/purple";
        break;
    case PLAT_BRICK:
        sprite = "markers/platform/brick/normal";
        break;
    case PLAT_GRASS:
        sprite = "markers/platform/grass/normal";
        break;
    case PLAT_GRASS_SMALL:
        sprite = "markers/platform/grass/small";
        break;
    case PLAT_BRICK_BIG:
        sprite = "markers/platform/brick/big";
        break;
    case PLAT_BRICK_TALL:
        sprite = "markers/platform/brick/tall";
        break;
    case PLAT_BRICK_SMALL:
        sprite = "markers/platform/brick/small";
        break;
    case PLAT_BRICK_BUTTONS:
        sprite = "markers/platform/brick/buttons";
        break;
    case PLAT_BLOCK:
        sprite = "markers/platform/block";
        break;
    }

    if (((ANY_FLAG(actor, FLG_PLATFORM_FALLING) && VAL(actor, PLATFORM_RESPAWN) >= 100)
            || (ANY_FLAG(actor, FLG_PLATFORM_RUNNING) && VAL(actor, PLATFORM_RESPAWN) >= 400))
        && (VAL(actor, PLATFORM_RESPAWN) % 2) == 0)
    {
        const Sint32 ax = Fx2Int(VAL(actor, PLATFORM_START_X)), ay = Fx2Int(VAL(actor, PLATFORM_START_Y));
        batch_pos(B_F3(ax, ay, Fx2Float(actor->depth)));
        batch_sprite(sprite);
        return;
    }

    Bool antijitter = FALSE;
    const GamePlayer* player = get_player(viewplayer());
    if (player != NULL
        && ((actor->vel.x != Fx0 && (player->bounds.end.x - player->bounds.start.x) > F_SCREEN_WIDTH)
            || (actor->vel.y != Fx0 && (player->bounds.end.y - player->bounds.start.y) > F_SCREEN_HEIGHT)))
    {
        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->platform == actor->id)
            antijitter = TRUE;
    }

    draw_actor(actor, sprite, antijitter);
}

static void on_top(GameActor* actor, GameActor* from) {
    from->platform = actor->id;

    if (from->type != ACT_PLAYER)
        return;

    if (ANY_FLAG(actor, FLG_PLATFORM_FALL) && !ANY_FLAG(actor, FLG_PLATFORM_FALLING)) {
        FLAG_OFF(actor, FLG_PLATFORM_WRAP | FLG_PLATFORM_RUN);
        FLAG_ON(actor, FLG_PLATFORM_FALLING);
    }

    if (ANY_FLAG(actor, FLG_PLATFORM_RUN) && !ANY_FLAG(actor, FLG_PLATFORM_RUNNING)) {
        actor->vel.x = VAL(actor, PLATFORM_RUN_X_SPEED);
        actor->vel.y = VAL(actor, PLATFORM_RUN_Y_SPEED);
        FLAG_ON(actor, FLG_PLATFORM_RUNNING);
    }

    if (ANY_FLAG(actor, FLG_PLATFORM_RUNNING))
        VAL(actor, PLATFORM_RESPAWN) = 0;
}

const ActorTable TAB_PLATFORM = {
    .is_solid = is_solid,
    .load_special = load_special,
    .create = create,
    .pre_tick = pre_tick,
    .draw = draw,
    .on_top = on_top,
};

/* ===============
   PLATFORM TURNER
   =============== */

static void create_turn(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void collide_turn(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLATFORM)
        return;

    if (ANY_FLAG(actor, FLG_PLATFORM_TURN_ADD)) {
        from->vel.x += actor->vel.x;
        from->vel.y += actor->vel.y;
    } else {
        from->vel.x = actor->vel.x;
        from->vel.y = actor->vel.y;
    }

    FLAG_OFF(from, VAL(actor, PLATFORM_TURN_FLAGS_OFF));
    FLAG_ON(from, VAL(actor, PLATFORM_TURN_FLAGS_ON));
}

const ActorTable TAB_PLATFORM_TURN = {
    .create = create_turn,
    .collide = collide_turn,
};
