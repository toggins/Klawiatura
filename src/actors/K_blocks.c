#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_effects.h"
#include "actors/K_powerups.h"
#include "actors/K_projectiles.h"

typedef Uint8 BlockTypes;
enum {
    BLOCK_ITEM,
    BLOCK_BRICK,
};

enum {
    VAL_BLOCK_TYPE,
    VAL_BLOCK_ITEM,
    VAL_BLOCK_BUMP,
    VAL_BLOCK_TIME,
};

#define FLG_BLOCK_REPEAT CUSTOM_FLAG(0)
#define FLG_BLOCK_HIDDEN CUSTOM_FLAG(1)
#define FLG_BLOCK_GRAY CUSTOM_FLAG(2)
#define FLG_BLOCK_EMPTY CUSTOM_FLAG(3)

static Bool bump_block(GameActor* actor, GameActor* from, Bool strong) {
    if (actor == NULL || actor->type != ACT_BLOCK || ANY_FLAG(actor, FLG_BLOCK_EMPTY))
        return FALSE;

    if (ANY_FLAG(actor, FLG_BLOCK_REPEAT)) {
        if (VAL(actor, BLOCK_TIME) <= 0) {
            VAL(actor, BLOCK_TIME) = 1;
        } else if (from != NULL && from->type == ACT_PLAYER && VAL(actor, BLOCK_BUMP) > 0
                   && VAL(actor, BLOCK_TIME) <= 301)
        {
            return FALSE;
        }
    } else if (VAL(actor, BLOCK_TYPE) == BLOCK_BRICK && VAL(actor, BLOCK_ITEM) == ACT_NULL) {
        if (strong) {
            if (from != NULL) {
                GamePlayer* player = get_player(from->player);
                if (player != NULL)
                    player->score += 50;
            }

            GameActor* shard = create_actor(ACT_BRICK_SHARD, Vadd(actor->pos, (FVec2){Int2Fx(22), Int2Fx(10)}));
            if (shard != NULL) {
                shard->vel.x = Int2Fx(2);
                shard->vel.y = Int2Fx(-8);
                if (ANY_FLAG(actor, FLG_BLOCK_GRAY))
                    FLAG_ON(shard, FLG_EFFECT_ALT);
            }
            shard = create_actor(ACT_BRICK_SHARD, Vadd(actor->pos, (FVec2){Int2Fx(21), Int2Fx(21)}));
            if (shard != NULL) {
                shard->vel.x = Int2Fx(4);
                shard->vel.y = Int2Fx(-7);
                if (ANY_FLAG(actor, FLG_BLOCK_GRAY))
                    FLAG_ON(shard, FLG_EFFECT_ALT);
            }
            shard = create_actor(ACT_BRICK_SHARD, Vadd(actor->pos, (FVec2){Int2Fx(10), Int2Fx(22)}));
            if (shard != NULL) {
                shard->vel.x = Int2Fx(-4);
                shard->vel.y = Int2Fx(-7);
                if (ANY_FLAG(actor, FLG_BLOCK_GRAY))
                    FLAG_ON(shard, FLG_EFFECT_ALT);
            }
            shard = create_actor(ACT_BRICK_SHARD, Vadd(actor->pos, (FVec2){Int2Fx(8), Int2Fx(9)}));
            if (shard != NULL) {
                shard->vel.x = Int2Fx(-2);
                shard->vel.y = Int2Fx(-8);
                if (ANY_FLAG(actor, FLG_BLOCK_GRAY))
                    FLAG_ON(shard, FLG_EFFECT_ALT);
            }

            play_state_sound("break", PLAY_POS, A_ACTOR(actor));

            FLAG_ON(actor, FLG_DESTROY);
        } else {
            VAL(actor, BLOCK_BUMP) = 1;
            play_state_sound("bump", PLAY_POS, A_ACTOR(actor));
        }

        GameActor* bump = create_actor(ACT_BLOCK_BUMP, actor->pos);
        if (bump != NULL && from != NULL)
            bump->player = from->player;

        return TRUE;
    }

    VAL(actor, BLOCK_BUMP) = 1;

    GameActor* bump = create_actor(ACT_BLOCK_BUMP, actor->pos);
    if (bump != NULL && from != NULL)
        bump->player = from->player;

    if (VAL(actor, BLOCK_ITEM) == ACT_NULL || ANY_FLAG(actor, FLG_BLOCK_EMPTY)) {
        play_state_sound("bump", PLAY_POS, A_ACTOR(actor));
        return TRUE;
    }

    ActorType item_type = VAL(actor, BLOCK_ITEM);
    switch (item_type) {
    default:
        break;

    case ACT_FIRE_FLOWER:
    case ACT_BEETROOT:
    case ACT_GREEN_LUI: {
        if (from != NULL) {
            GamePlayer* player = get_player(from->player);
            if (player != NULL && player->powerup == POW_NONE)
                item_type = ACT_SUPER_MUSHROOM;
        }

        break;
    }
    }

    GameActor* item = create_actor(item_type, actor->pos);
    if (item != NULL) {
        if (from != NULL)
            item->player = from->player;

        move_actor(item, Vadd(actor->pos, (FVec2){Flerp(actor->box.start.x, actor->box.end.x, FxHalf)
                                                      - Flerp(item->box.start.x, item->box.end.x, FxHalf),
                                              -item->box.end.y}));
        skip_interp(item);

        switch (item->type) {
        case ACT_SUPER_MUSHROOM:
        case ACT_1UP_MUSHROOM:
        case ACT_POISON_MUSHROOM: {
            item->vel.x = Int2Fx(2);
        }
        case ACT_FIRE_FLOWER:
        case ACT_BEETROOT:
        case ACT_GREEN_LUI:
        case ACT_STARMAN: {
            FLAG_ON(item, FLG_POWERUP_SPROUTED);
        }
        default: {
            item->sprout = 32;
            play_state_sound("sprout", PLAY_POS, A_ACTOR(item));
            break;
        }

        case ACT_COIN_POP:
            break;
        }
    }

    if (!ANY_FLAG(actor, FLG_BLOCK_REPEAT) || VAL(actor, BLOCK_TIME) > 301)
        FLAG_ON(actor, FLG_BLOCK_EMPTY);

    return TRUE;
}

/* =====
   BLOCK
   ===== */

static SolidFlags is_solid(const GameActor* actor) {
    return ANY_FLAG(actor, FLG_BLOCK_HIDDEN) ? SOL_BOTTOM : SOL_SOLID;
}

static void load() {
    load_actor(ACT_BLOCK_BUMP);
}

static void load_special(const GameActor* actor) {
    if (VAL(actor, BLOCK_TYPE) == BLOCK_BRICK) {
        load_sprite(ANY_FLAG(actor, FLG_BLOCK_GRAY) ? "items/block/brick_gray" : "items/block/brick", AKL_NEVER);

        if (VAL(actor, BLOCK_ITEM) == ACT_NULL && !ANY_FLAG(actor, FLG_BLOCK_REPEAT)) {
            load_sound("break", AKL_NEVER);
            load_actor(ACT_BRICK_SHARD);
        } else {
            load_sprite("items/block/empty", AKL_NEVER);
        }
    } else {
        load_sprite_num("items/block/%u", 3, AKL_NEVER);
        load_sprite("items/block/empty", AKL_NEVER);
    }

    if (VAL(actor, BLOCK_ITEM) == ACT_NULL) {
        load_sound("bump", AKL_NEVER);
    } else {
        if (VAL(actor, BLOCK_ITEM) != ACT_COIN_POP)
            load_sound("sprout", AKL_NEVER);

        load_actor(VAL(actor, BLOCK_ITEM));
        switch (VAL(actor, BLOCK_ITEM)) {
        default:
            break;
        case ACT_FIRE_FLOWER:
        case ACT_BEETROOT:
        case ACT_GREEN_LUI:
            load_actor(ACT_SUPER_MUSHROOM);
            break;
        }
    }
}

static void create(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    actor->depth = Int2Fx(19);
}

static void pre_tick(GameActor* actor) {
    if (VAL(actor, BLOCK_BUMP) > 0) {
        ++VAL(actor, BLOCK_BUMP);
        if (VAL(actor, BLOCK_BUMP)
            > ((VAL(actor, BLOCK_TYPE) == BLOCK_BRICK && ANY_FLAG(actor, FLG_BLOCK_EMPTY)) ? 11 : 10))
        {
            VAL(actor, BLOCK_BUMP) = 0;
        }
    }

    if (VAL(actor, BLOCK_TIME) >= 1 && VAL(actor, BLOCK_TIME) <= 301)
        ++VAL(actor, BLOCK_TIME);
}

static void draw(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_BLOCK_HIDDEN))
        return;

    batch_reset();

    Uint8 bump = 0;
    if (VAL(actor, BLOCK_BUMP) > 0) {
        switch (VAL(actor, BLOCK_TYPE)) {
        default: {
            switch (VAL(actor, BLOCK_BUMP)) {
            default:
                break;
            case 2:
            case 9:
                bump = 1;
                break;
            case 3:
            case 8:
                bump = 2;
                break;
            case 4:
            case 7:
                bump = 3;
                break;
            case 5:
            case 6:
                bump = 4;
                break;
            }

            break;
        }

        case BLOCK_BRICK: {
            if (ANY_FLAG(actor, FLG_BLOCK_EMPTY)) {
                switch (VAL(actor, BLOCK_BUMP)) {
                default:
                    break;
                case 2:
                    bump = 2;
                    break;
                case 3:
                    bump = 4;
                    break;
                case 4:
                    bump = 6;
                    break;
                case 5:
                case 8:
                    bump = 7;
                    break;
                case 6:
                case 7:
                    bump = 8;
                    break;
                case 9:
                    bump = 5;
                    break;
                case 10:
                    bump = 3;
                    break;
                }
            } else {
                switch (VAL(actor, BLOCK_BUMP)) {
                default:
                    break;
                case 2:
                case 10:
                    bump = 3;
                    break;
                case 3:
                case 9:
                    bump = 6;
                    break;
                case 4:
                case 8:
                    bump = 8;
                    break;
                case 5:
                case 7:
                    bump = 10;
                    break;
                case 6:
                    bump = 11;
                    break;
                }
            }

            break;
        }
        }
    }
    batch_offset(B_F3_XY(0.f, bump));

    const char* sprite = "items/block/empty";
    if (!ANY_FLAG(actor, FLG_BLOCK_EMPTY)) {
        switch (VAL(actor, BLOCK_TYPE)) {
        default:
            sprite = fmt("items/block/%i", ((gamestate()->time * 9) / 100) % 3);
            break;
        case BLOCK_BRICK:
            sprite = ANY_FLAG(actor, FLG_BLOCK_GRAY) ? "items/block/brick_gray" : "items/block/brick";
            break;
        }
    }
    draw_actor(actor, sprite, FALSE);
}

static void on_other_sides(GameActor* actor, GameActor* from) {
    if (from->type != ACT_BEETROOT_PROJECTILE)
        return;

    const GamePlayer* player = get_player(from->player);
    if ((player != NULL || (VAL(actor, BLOCK_TYPE) == BLOCK_BRICK && VAL(actor, BLOCK_ITEM) == ACT_NULL))
        && bump_block(actor, from, TRUE))
    {
        FLAG_ON(from, FLG_PROJECTILE_HIT_BLOCK);
    }
}

static void on_bottom(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        FLAG_OFF(actor, FLG_BLOCK_HIDDEN);
        const GamePlayer* player = get_player(from->player);
        bump_block(actor, from, player != NULL && player->powerup != POW_NONE);

        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        if (!ANY_FLAG(actor, FLG_BLOCK_HIDDEN))
            on_other_sides(actor, from);

        break;
    }
    }
}

const ActorTable TAB_BLOCK = {
    .is_solid = is_solid,
    .load = load,
    .load_special = load_special,
    .create = create,
    .pre_tick = pre_tick,
    .draw = draw,
    .on_top = on_other_sides,
    .on_left = on_other_sides,
    .on_bottom = on_bottom,
    .on_right = on_other_sides,
};

/* ==========
   BLOCK BUMP
   ========== */

static void create_bump(GameActor* actor) {
    actor->box.start.y = Int2Fx(-8);
    actor->box.end.x = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_bump(GameActor* actor) {
    if (VAL(actor, BLOCK_BUMP) > 6) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    collide_actor(actor);
    ++VAL(actor, BLOCK_BUMP);
}

const ActorTable TAB_BLOCK_BUMP = {
    .create = create_bump,
    .tick = tick_bump,
};
