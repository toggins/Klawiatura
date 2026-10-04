#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_checkpoint.h"
#include "actors/K_platform.h"

/* ========
   PLATFORM
   ======== */

static void init_platform(GameActor* actor) {
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

    case PLAT_BIG: {
        actor->box.start.x = actor->box.start.y = Fx0;
        actor->box.end.x = Int2Fx(128);
        actor->box.end.y = Int2Fx(16);

        break;
    }

    case PLAT_CLOUD_BIG: {
        actor->box.start.x = actor->box.start.y = Fx0;
        actor->box.end.x = Int2Fx(254);
        actor->box.end.y = Int2Fx(32);

        break;
    }
    }

    if (ANY_FLAG(actor, FLG_PLATFORM_RUN)) {
        const GameActor* checkpoint = get_actor(gamestate()->checkpoint);
        if (checkpoint != NULL && ANY_FLAG(checkpoint, FLG_CHECKPOINT_SET_PLATFORM)) {
            move_actor(actor, (FVec2){VAL(checkpoint, CHECKPOINT_PLATFORM_X), VAL(checkpoint, CHECKPOINT_PLATFORM_Y)});
            actor->last_pos = actor->pos;
            skip_interp(actor);
        }
    }

    VAL(actor, PLATFORM_START_X) = actor->pos.x;
    VAL(actor, PLATFORM_START_Y) = actor->pos.y;
    VAL(actor, PLATFORM_START_X_SPEED) = actor->vel.x;
    VAL(actor, PLATFORM_START_Y_SPEED) = actor->vel.y;
    VAL(actor, PLATFORM_START_FLAGS) = (ActorValue)actor->flags;
    VAL(actor, PLATFORM_START_MASK) = VAL(actor, PLATFORM_MASK);
}

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
    case PLAT_BIG:
        load_sprite("markers/platform/big", AKL_NEVER);
        break;
    case PLAT_CLOUD_BIG:
        load_sprite_num("markers/platform/cloud/big/%u", 4, AKL_NEVER);
        break;
    }
}

static void create(GameActor* actor) {
    actor->depth = Int2Fx(19);
}

static void pre_tick(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (game_state->time == 0)
        init_platform(actor);

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
        VAL(actor, PLATFORM_MASK) = VAL(actor, PLATFORM_START_MASK);                                                   \
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
    case PLAT_BIG:
        sprite = "markers/platform/big";
        break;
    case PLAT_CLOUD_BIG:
        sprite = fmt("markers/platform/cloud/big/%i", ((gamestate()->time * 2) / 25) % 4);
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
    if (get_actor(gamestate()->autoscroll) == NULL) {
        const GamePlayer* player = get_player(viewplayer());
        if (player != NULL
            && ((actor->vel.x != Fx0 && (player->bounds.end.x - player->bounds.start.x) > F_SCREEN_WIDTH)
                || (actor->vel.y != Fx0 && (player->bounds.end.y - player->bounds.start.y) > F_SCREEN_HEIGHT)))
        {
            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->platform == actor->id)
                antijitter = TRUE;
        }
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
    switch (from->type) {
    default:
        break;

    case ACT_PLATFORM: {
        if (VAL(from, PLATFORM_MASK) != VAL(actor, PLATFORM_TURN_MASK))
            break;

        if (ANY_FLAG(actor, FLG_PLATFORM_TURN_ADD)) {
            from->vel.x += actor->vel.x;
            from->vel.y += actor->vel.y;
        } else {
            from->vel.x = actor->vel.x;
            from->vel.y = actor->vel.y;
        }

        VAL(from, PLATFORM_MASK) = VAL(actor, PLATFORM_TURN_SET_MASK);
        FLAG_OFF(from, VAL(actor, PLATFORM_TURN_FLAGS_OFF));
        FLAG_ON(from, VAL(actor, PLATFORM_TURN_FLAGS_ON));

        break;
    }

    case ACT_CENTIPEDE: {
        if (VAL(from, CENTIPEDE_MASK) != VAL(actor, PLATFORM_TURN_MASK))
            break;

        if (from->vel.x != actor->vel.x || from->vel.y != actor->vel.y) {
            const FRect abox = Radd(actor->box, actor->pos), fbox = Radd(from->box, from->pos);
            const FVec2 overlap = {Fmin(fbox.end.x, abox.end.x) - Fmax(fbox.start.x, abox.start.x),
                Fmin(fbox.end.y, abox.end.y) - Fmax(fbox.start.y, abox.start.y)};

            FVec2 push = from->pos;
            if (overlap.x < overlap.y)
                push.x += (Rcenter(fbox).x < Rcenter(abox).x) ? -overlap.x : overlap.x;
            else
                push.y += (Rcenter(fbox).y < Rcenter(abox).y) ? -overlap.y : overlap.y;
            move_actor(from, push);

            from->vel = actor->vel;
            VAL(from, CENTIPEDE_WAIT) = VAL(from, CENTIPEDE_DELAY);
        }

        VAL(from, CENTIPEDE_MASK) = VAL(actor, PLATFORM_TURN_SET_MASK);

        break;
    }
    }
}

const ActorTable TAB_PLATFORM_TURN = {
    .create = create_turn,
    .collide = collide_turn,
};

/* =========
   CENTIPEDE
   ========= */

static void create_centipede_trail(GameActor* actor) {
    GameActor* platform = create_actor(ACT_PLATFORM, actor->pos);
    if (platform != NULL) {
        platform->depth = 1310719;
        VAL(platform, PLATFORM_TYPE) = PLAT_BRICK_SMALL;
        VAL(platform, PLATFORM_MASK) = FxUpper;
        init_platform(platform);

        align_interp(platform, actor);
    }
}

static void load_centipede() {
    load_sprite("markers/platform/brick/small", AKL_NEVER);
    load_actor(ACT_CENTIPEDE_WHEEL);
}

static void create_centipede(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    actor->depth = Int2Fx(18);

    VAL(actor, CENTIPEDE_X_SPEED) = Int2Fx(2);
    VAL(actor, CENTIPEDE_DELAY) = 16;
    VAL(actor, CENTIPEDE_WHEEL) = NULL_ACTOR;
}

static void cleanup_centipede(GameActor* actor) {
    GameActor* wheel = get_actor(VAL(actor, CENTIPEDE_WHEEL));
    if (wheel != NULL)
        FLAG_ON(wheel, FLG_DESTROY);
}

static void pre_tick_centipede(GameActor* actor) {
    if (gamestate()->time == 0)
        collide_actor(actor);

    if (!ANY_FLAG(actor, FLG_CENTIPEDE_ACTIVE)) {
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player == NULL)
                continue;

            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER && pawn->pos.x > (actor->pos.x - Int2Fx(36))
                && pawn->pos.x < (actor->pos.x + Int2Fx(68)))
            {
                actor->vel.x = VAL(actor, CENTIPEDE_X_SPEED);
                actor->vel.y = VAL(actor, CENTIPEDE_Y_SPEED);
                VAL(actor, CENTIPEDE_WAIT) += VAL(actor, CENTIPEDE_DELAY);
                FLAG_ON(actor, FLG_CENTIPEDE_ACTIVE);

                break;
            }
        }
    }

    GameActor* wheel = get_actor(VAL(actor, CENTIPEDE_WHEEL));
    if (wheel == NULL) {
        wheel = create_actor(ACT_CENTIPEDE_WHEEL, Vadd(actor->pos, (FVec2){Int2Fx(16), Int2Fx(32)}));
        if (wheel != NULL)
            VAL(actor, CENTIPEDE_WHEEL) = wheel->id;
    }

    if (actor->vel.x != Fx0 || actor->vel.y != Fx0) {
        if (wheel != NULL)
            VAL(wheel, CENTIPEDE_WHEEL) = Fmod(VAL(wheel, CENTIPEDE_WHEEL) + 12868, Fx2Pi);

        if (VAL(actor, CENTIPEDE_WAIT) > 0) {
            if (--VAL(actor, CENTIPEDE_WAIT) <= 0) {
                if (ANY_FLAG(actor, FLG_CENTIPEDE_TRAIL) && !ANY_FLAG(actor, FLG_CENTIPEDE_CHILD))
                    create_centipede_trail(actor);
            }
        }

        if (VAL(actor, CENTIPEDE_WAIT) <= 0) {
            move_actor(actor, Vadd(actor->pos, actor->vel));
            collide_actor(actor);
            push_actors(actor);
        }

        if (!ANY_FLAG(actor, FLG_CENTIPEDE_CHILD)) {
            while (--VAL(actor, CENTIPEDE_SOUND) <= 0) {
                VAL(actor, CENTIPEDE_SOUND) += 247;

                play_state_sound("centipede", PLAY_ACTOR, A_ATTACH(actor));
            }
        }
    }

    if (wheel != NULL) {
        move_actor(wheel, Vadd(actor->pos, (FVec2){Int2Fx(16), Int2Fx(32)}));
        wheel->depth = actor->depth + Fx1;
    }
}

static void draw_centipede(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "markers/platform/brick/small", FALSE);
}

static void collide_centipede(GameActor* actor, GameActor* from) {
    if (from->type == ACT_CENTIPEDE && gamestate()->time <= 0 && from->id > actor->id) {
        --actor->depth;
        VAL(actor, CENTIPEDE_WAIT) += VAL(actor, CENTIPEDE_DELAY);
        FLAG_ON(actor, FLG_CENTIPEDE_CHILD);
    }
}

static void on_top_centipede(GameActor* actor, GameActor* from) {
    if (Fabs(actor->vel.y) >= Fabs(actor->vel.x))
        from->platform = actor->id;
}

const ActorTable TAB_CENTIPEDE = {
    .is_solid = always_solid,
    .load = load_centipede,
    .create = create_centipede,
    .cleanup = cleanup_centipede,
    .pre_tick = pre_tick_centipede,
    .draw = draw_centipede,
    .collide = collide_centipede,
    .on_top = on_top_centipede,
};

/* ===============
   CENTIPEDE WHEEL
   =============== */

static void load_centipede_wheel() {
    load_sprite("markers/platform/wheel", AKL_NEVER);
    load_sound("centipede", AKL_NEVER);
}

static void draw_centipede_wheel(const GameActor* actor) {
    batch_reset();
    batch_angle(Fx2Float(VAL(actor, CENTIPEDE_WHEEL)));
    draw_actor(actor, "markers/platform/wheel", FALSE);
}

const ActorTable TAB_CENTIPEDE_WHEEL = {
    .load = load_centipede_wheel,
    .draw = draw_centipede_wheel,
};
