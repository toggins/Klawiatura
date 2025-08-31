#include "K_game.h"
#include "K_audio.h"
#include "K_log.h"
#include "K_file.h"

static struct GameState state = {0};
static PlayerID local_player = -1L;

static struct InterpObject interp[MAX_OBJECTS] = {0};

/* ====

   GAME

   ==== */

static ObjectID find_object(GameObjectType type) {
    ObjectID oid = state.live_objects;
    while (oid >= 0L) {
        const struct GameObject* object = &(state.objects[oid]);
        if (object->type == type)
            return oid;
        oid = object->previous;
    }
    return NULLOBJ;
}

static struct GamePlayer* get_player(PlayerID pid) {
    return (pid < 0L || pid >= MAX_PLAYERS) ? NULL : &(state.players[pid]);
}

static ObjectID nearest_player(fvec2 pos) {
    ObjectID nearest = NULLOBJ;
    fix16_t distance = FxZero;

    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        if (!(player->active))
            continue;

        struct GameObject* pawn = get_object(player->object);
        if (pawn == NULL)
            continue;

        const fix16_t ndist = Fsqrt(Fadd(Fsqr(Fsub(pawn->pos[0], pos[0])), Fsqr(Fsub(pawn->pos[1], pos[1]))));
        if (nearest == NULLOBJ || ndist < distance) {
            nearest = player->object;
            distance = ndist;
        }
    }

    return nearest;
}

static ObjectID respawn_player(PlayerID pid) {
    struct GamePlayer* player = get_player(pid);
    if (player == NULL || !(player->active) || player->lives <= 0L || state.clock == 0L ||
        state.sequence.type != SEQ_NONE)
        return NULLOBJ;

    kill_object(player->object);
    player->object = NULLOBJ;

    struct Kevin* kevin = &(player->kevin);
    kill_object(kevin->object);
    kevin->object = NULLOBJ;

    const struct GameObject* spawn = get_object(state.autoscroll);
    if (spawn == NULL)
        spawn = get_object(state.checkpoint);
    if (spawn == NULL)
        spawn = get_object(state.spawn);
    if (spawn == NULL)
        return NULLOBJ;

    if (spawn->type == OBJ_CHECKPOINT &&
        spawn->values[VAL_CHECKPOINT_BOUNDS_X1] != spawn->values[VAL_CHECKPOINT_BOUNDS_X2] &&
        spawn->values[VAL_CHECKPOINT_BOUNDS_Y1] != spawn->values[VAL_CHECKPOINT_BOUNDS_Y2]) {
        player->bounds[0][0] = spawn->values[VAL_CHECKPOINT_BOUNDS_X1];
        player->bounds[0][1] = spawn->values[VAL_CHECKPOINT_BOUNDS_Y1];
        player->bounds[1][0] = spawn->values[VAL_CHECKPOINT_BOUNDS_X2];
        player->bounds[1][1] = spawn->values[VAL_CHECKPOINT_BOUNDS_Y2];
    } else {
        player->bounds[0][0] = state.bounds[0][0];
        player->bounds[0][1] = state.bounds[0][1];
        player->bounds[1][0] = state.bounds[1][0];
        player->bounds[1][1] = state.bounds[1][1];
    }

    player->object = create_object(
        OBJ_PLAYER, (spawn->type == OBJ_AUTOSCROLL)
                        ? (fvec2){Flerp(state.bounds[0][0], state.bounds[1][0], FxHalf), state.bounds[0][1]}
                        : spawn->pos
    );
    struct GameObject* pawn = get_object(player->object);
    if (pawn != NULL) {
        pawn->values[VAL_PLAYER_INDEX] = (fix16_t)pid;
        pawn->values[VAL_PLAYER_KEVIN_X] = pawn->pos[0];
        pawn->values[VAL_PLAYER_KEVIN_Y] = pawn->pos[1];

        pawn->flags |= (spawn->flags & (FLG_X_FLIP | FLG_PLAYER_ASCEND | FLG_PLAYER_DESCEND));
        if (pawn->flags & FLG_PLAYER_ASCEND)
            pawn->values[VAL_Y_SPEED] = Fsub(pawn->values[VAL_Y_SPEED], FfInt(22L));

        if (spawn->type == OBJ_AUTOSCROLL)
            pawn->flags |= FLG_PLAYER_RESPAWN;
    }
    return player->object;
}

static void give_points(struct GameObject* item, PlayerID pid, int32_t points) {
    struct GamePlayer* player = get_player(pid);
    if (player == NULL)
        return;

    if (points < 0L) {
        player->lives -= points;
        if (pid == local_player)
            play_sound("1UP");
    } else
        player->score += points;

    struct GameObject* pts =
        get_object(create_object(OBJ_POINTS, (fvec2){item->pos[0], Fadd(item->pos[1], item->bbox[0][1])}));
    if (pts != NULL) {
        pts->values[VAL_POINTS_PLAYER] = (fix16_t)pid;
        pts->values[VAL_POINTS] = points;
    }
}

static Bool is_solid(ObjectID oid, Bool ignore_full, Bool ignore_top) {
    switch (state.objects[oid].type) {
        case OBJ_SOLID:
        case OBJ_SOLID_SLOPE:
        case OBJ_ITEM_BLOCK:
        case OBJ_BRICK_BLOCK:
        case OBJ_BRICK_BLOCK_COIN:
        case OBJ_PSWITCH_BRICK:
        case OBJ_WHEEL_LEFT:
        case OBJ_WHEEL:
        case OBJ_WHEEL_RIGHT:
            return !ignore_full;
        case OBJ_SOLID_TOP:
            return !ignore_top;
        default:
            return false;
    }
}

static Bool touching_solid(const fvec2 rect[2]) {
    int32_t bx1 = Fsub(rect[0][0], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by1 = Fsub(rect[0][1], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t bx2 = Fadd(rect[1][0], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by2 = Fadd(rect[1][1], BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);

    for (int32_t by = by1; by <= by2; by++)
        for (int32_t bx = bx1; bx <= bx2; bx++) {
            ObjectID oid = state.blockmap[bx + (by * MAX_BLOCKS)];
            while (object_is_alive(oid)) {
                const struct GameObject* other = &(state.objects[oid]);
                if (is_solid(oid, false, true)) {
                    const fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                    const fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                    const fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                    const fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                    if (rect[0][0] < ox2 && rect[1][0] > ox1 && rect[0][1] < oy2 && rect[1][1] > oy1)
                        return true;
                }
                oid = other->previous_block;
            }
        }

    return false;
}

static PlayerID get_owner_id(ObjectID oid) {
    const struct GameObject* object = get_object(oid);
    if (object == NULL)
        return -1L;

    switch (object->type) {
        default:
            return -1L;

        case OBJ_PLAYER:
        case OBJ_PLAYER_DEAD:
            return (PlayerID)(object->values[VAL_PLAYER_INDEX]);

        case OBJ_KEVIN:
            return (PlayerID)(object->values[VAL_KEVIN_PLAYER]);

        case OBJ_MISSILE_FIREBALL:
        case OBJ_MISSILE_BEETROOT:
        case OBJ_MISSILE_BEETROOT_SINK:
        case OBJ_MISSILE_HAMMER:
            return get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));

        case OBJ_BLOCK_BUMP:
            return get_owner_id((ObjectID)(object->values[VAL_BLOCK_BUMP_OWNER]));

        case OBJ_COIN_POP:
            return get_owner_id((ObjectID)(object->values[VAL_COIN_POP_OWNER]));

        case OBJ_POINTS:
            return (PlayerID)(object->values[VAL_POINTS_PLAYER]);

        case OBJ_ROTODISC:
            return get_owner_id((ObjectID)(object->values[VAL_ROTODISC_OWNER]));
    }
}

static struct GamePlayer* get_owner(ObjectID oid) {
    return get_player(get_owner_id(oid));
}

static void bump_block(struct GameObject* block, ObjectID from, Bool strong) {
    if ((block->flags & FLG_BLOCK_EMPTY) ||
        (object_is_alive(from) && state.objects[from].type == OBJ_PLAYER && block->values[VAL_BLOCK_BUMP] > 0L))
        return;

    switch (block->type) {
        default:
            break;

        case OBJ_ITEM_BLOCK:
        case OBJ_BRICK_BLOCK_COIN: {
            Bool is_powerup = false;
            switch (block->values[VAL_BLOCK_ITEM]) {
                default:
                    break;

                case OBJ_MUSHROOM:
                case OBJ_FIRE_FLOWER:
                case OBJ_BEETROOT:
                case OBJ_LUI:
                case OBJ_HAMMER_SUIT: {
                    const struct GamePlayer* player = get_owner(from);
                    if (player != NULL && player->power == POW_SMALL)
                        block->values[VAL_BLOCK_ITEM] = OBJ_MUSHROOM;
                    else
                        is_powerup = true;
                    break;
                }
            }

            const ObjectID iid = create_object(block->values[VAL_BLOCK_ITEM], block->pos);
            if (object_is_alive(iid)) {
                struct GameObject* item = &(state.objects[iid]);

                move_object(
                    iid, (fvec2){Fsub(
                                     Fadd(block->pos[0], Flerp(block->bbox[0][0], block->bbox[1][0], FxHalf)),
                                     Flerp(item->bbox[0][0], item->bbox[1][0], FxHalf)
                                 ),
                                 Fsub(block->pos[1], item->bbox[1][1])}
                );
                skip_interp(iid);

                if (item->type == OBJ_COIN_POP) {
                    item->values[VAL_COIN_POP_OWNER] = from;
                } else {
                    item->values[VAL_SPROUT] = 32L;
                    switch (item->type) {
                        default:
                            break;

                        case OBJ_MUSHROOM:
                        case OBJ_MUSHROOM_1UP:
                            item->values[VAL_X_SPEED] = FfInt(2L);
                            break;

                        case OBJ_STARMAN:
                            item->values[VAL_X_SPEED] = 0x00028000;
                            break;
                    }

                    if (is_powerup)
                        item->flags &= ~FLG_POWERUP_FULL;
                    play_sound_at_object(item, "SPROUT");
                }
            }

            block->values[VAL_BLOCK_BUMP] = 1L;
            if (block->type == OBJ_ITEM_BLOCK) {
                block->flags |= FLG_BLOCK_EMPTY;
            } else {
                if (block->values[VAL_BLOCK_TIME] <= 0L)
                    block->values[VAL_BLOCK_TIME] = 1L;
                else if (block->values[VAL_BLOCK_TIME] > 300L)
                    block->flags |= FLG_BLOCK_EMPTY;
            }
            break;
        }

        case OBJ_BRICK_BLOCK:
        case OBJ_PSWITCH_BRICK: {
            if (strong) {
                struct GameObject* shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(8)), Fadd(block->pos[1], FfInt(8))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(-2);
                    shard->values[VAL_Y_SPEED] = FfInt(-8);
                    shard->flags |= (block->flags & FLG_BLOCK_GRAY);
                }

                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(16)), Fadd(block->pos[1], FfInt(8))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(2);
                    shard->values[VAL_Y_SPEED] = FfInt(-8);
                    shard->flags |= (block->flags & FLG_BLOCK_GRAY);
                }

                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(8)), Fadd(block->pos[1], FfInt(16))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(-3);
                    shard->values[VAL_Y_SPEED] = FfInt(-6);
                    shard->flags |= (block->flags & FLG_BLOCK_GRAY);
                }

                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(16)), Fadd(block->pos[1], FfInt(16))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(3);
                    shard->values[VAL_Y_SPEED] = FfInt(-6);
                    shard->flags |= (block->flags & FLG_BLOCK_GRAY);
                }

                struct GamePlayer* player = get_owner(from);
                if (player != NULL)
                    player->score += 50L;

                play_sound_at_object(block, "BREAK");
                block->flags |= FLG_DESTROY;
            } else if (block->values[VAL_BLOCK_BUMP] <= 0L) {
                block->values[VAL_BLOCK_BUMP] = 1L;
                play_sound_at_object(block, "BUMP");
            }
            break;
        }
    }

    struct GameObject* bump = get_object(create_object(OBJ_BLOCK_BUMP, block->pos));
    if (bump != NULL)
        bump->values[VAL_BLOCK_BUMP_OWNER] = from;
}

static void top_check(ObjectID self_id, ObjectID other_id) {
    struct GameObject* self = &(state.objects[self_id]);
    switch (self->type) {
        default:
            break;
    }
}

static void bottom_check(ObjectID self_id, ObjectID other_id) {
    struct GameObject* self = &(state.objects[self_id]);
    switch (self->type) {
        default:
            break;

        case OBJ_ITEM_BLOCK:
        case OBJ_BRICK_BLOCK:
        case OBJ_BRICK_BLOCK_COIN:
        case OBJ_PSWITCH_BRICK: {
            const struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const struct GamePlayer* player = get_owner(other_id);
            bump_block(self, other_id, player != NULL && player->power != POW_SMALL);
            break;
        }
    }
}

static void displace_object(ObjectID did, fix16_t climb, Bool unstuck) {
    struct GameObject* displacee = get_object(did);
    if (displacee == NULL)
        return;

    displacee->values[VAL_X_TOUCH] = 0L;
    displacee->values[VAL_Y_TOUCH] = 0L;

    fix16_t x = displacee->pos[0];
    fix16_t y = displacee->pos[1];

    // Horizontal collision
    x = Fadd(x, displacee->values[VAL_X_SPEED]);
    struct BlockList list = {0};
    list_block_at(
        &list, (fvec2[2]){
                   {Fadd(x, displacee->bbox[0][0]), Fadd(y, displacee->bbox[0][1])},
                   {Fadd(x, displacee->bbox[1][0]), Fadd(y, displacee->bbox[1][1])},
               }
    );
    Bool climbed = false;

    if (list.num_objects > 0L) {
        Bool stop = false;
        if (displacee->values[VAL_X_SPEED] < FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, true))
                    continue;

                const struct GameObject* object = &(state.objects[oid]);
                if (object->type == OBJ_SOLID_SLOPE)
                    continue;

                if (displacee->values[VAL_Y_SPEED] >= FxZero &&
                    Fsub(Fadd(displacee->pos[1], displacee->bbox[1][1]), climb) <
                        Fadd(object->pos[1], object->bbox[0][1])) {
                    const fix16_t step = Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]);
                    if (!touching_solid((fvec2[2]){
                            {Fsub(Fadd(x, displacee->bbox[0][0]), FxOne),
                             Fsub(Fadd(step, displacee->bbox[0][1]), FxOne)},
                            {Fsub(Fadd(x, displacee->bbox[1][0]), FxOne),
                             Fsub(Fadd(step, displacee->bbox[1][1]), FxOne)},
                        })) {
                        y = step;
                        displacee->values[VAL_Y_SPEED] = FxZero;
                        displacee->values[VAL_Y_TOUCH] = 1L;
                        climbed = true;
                    }
                    continue;
                }

                x = Fmax(x, Fsub(Fadd(object->pos[0], object->bbox[1][0]), displacee->bbox[0][0]));
                stop = true;
                climbed = false;
            }
            displacee->values[VAL_X_TOUCH] = -(stop && !climbed);
        } else if (displacee->values[VAL_X_SPEED] > FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, true))
                    continue;

                const struct GameObject* object = &(state.objects[oid]);
                if (object->type == OBJ_SOLID_SLOPE)
                    continue;

                if (displacee->values[VAL_Y_SPEED] >= FxZero &&
                    Fsub(Fadd(displacee->pos[1], displacee->bbox[1][1]), climb) <
                        Fadd(object->pos[1], object->bbox[0][1])) {
                    const fix16_t step = Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]);
                    if (!touching_solid((fvec2[2]){
                            {Fadd(Fadd(x, displacee->bbox[0][0]), FxOne),
                             Fsub(Fadd(step, displacee->bbox[0][1]), FxOne)},
                            {Fadd(Fadd(x, displacee->bbox[1][0]), FxOne),
                             Fsub(Fadd(step, displacee->bbox[1][1]), FxOne)},
                        })) {
                        y = step;
                        displacee->values[VAL_Y_SPEED] = FxZero;
                        displacee->values[VAL_Y_TOUCH] = 1L;
                        climbed = true;
                    }
                    continue;
                }

                x = Fmin(x, Fsub(Fadd(object->pos[0], object->bbox[0][0]), displacee->bbox[1][0]));
                stop = true;
                climbed = false;
            }
            displacee->values[VAL_X_TOUCH] = stop && !climbed;
        }

        if (stop)
            displacee->values[VAL_X_SPEED] = FxZero;
    }

    // Vertical collision
    y = Fadd(y, displacee->values[VAL_Y_SPEED]);
    list_block_at(
        &list, (fvec2[2]){
                   {Fadd(x, displacee->bbox[0][0]), Fadd(y, displacee->bbox[0][1])},
                   {Fadd(x, displacee->bbox[1][0]), Fadd(y, displacee->bbox[1][1])},
               }
    );

    if (list.num_objects > 0L) {
        Bool stop = false;
        if (displacee->values[VAL_Y_SPEED] < FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, true))
                    continue;

                const struct GameObject* object = &(state.objects[oid]);
                if (object->type == OBJ_SOLID_SLOPE)
                    continue;

                y = Fmax(y, Fsub(Fadd(object->pos[1], object->bbox[1][1]), displacee->bbox[0][1]));
                stop = true;
                bottom_check(oid, did);
            }
            displacee->values[VAL_Y_TOUCH] = -(fix16_t)stop;
        } else if (displacee->values[VAL_Y_SPEED] > FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, false))
                    continue;

                const struct GameObject* object = &(state.objects[oid]);
                if (object->type == OBJ_SOLID_SLOPE) {
                    const Bool side = (object->flags & FLG_X_FLIP) == FLG_X_FLIP;
                    const fix16_t slope = Flerp(
                        Fadd(object->pos[1], object->bbox[!side][1]), Fadd(object->pos[1], object->bbox[side][1]),
                        Fclamp(
                            Fdiv(
                                Fsub(Fadd(x, displacee->bbox[!side][0]), Fadd(object->pos[0], object->bbox[side][0])),
                                Fsub(object->bbox[1][0], object->bbox[0][0])
                            ),
                            FxZero, FxOne
                        )
                    );

                    if (y >= slope) {
                        y = slope;
                        stop = true;
                    }
                    continue;
                }

                if (is_solid(oid, true, false) && Fsub(Fadd(y, displacee->bbox[1][1]), displacee->values[VAL_Y_SPEED]) >
                                                      Fadd(Fadd(object->pos[1], object->bbox[0][1]), climb))
                    continue;

                y = Fmin(y, Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]));
                stop = true;
                top_check(oid, did);
            }
            displacee->values[VAL_Y_TOUCH] = (fix16_t)stop;
        }

        if (stop)
            displacee->values[VAL_Y_SPEED] = FxZero;
    }

    move_object(did, (fvec2){x, y});
}

static void win_player(struct GameObject* pawn) {
    if (state.sequence.type == SEQ_WIN)
        return;
    state.sequence.type = SEQ_WIN;
    state.sequence.time = 1L;
    const PlayerID pid = state.sequence.activator = (PlayerID)(pawn->values[VAL_PLAYER_INDEX]);

    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        if (i != pid) {
            kill_object(player->object);
            player->object = NULLOBJ;
        }

        struct GameObject* kpawn = get_object(player->kevin.object);
        if (kpawn != NULL) {
            create_object(OBJ_EXPLODE, (fvec2){kpawn->pos[0], Fsub(kpawn->pos[1], FfInt(16L))});
            kpawn->flags |= FLG_DESTROY;
        }
        player->kevin.object = NULLOBJ;
    }

    for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++) {
        struct GameObject* missile = get_object((ObjectID)(pawn->values[i]));
        if (missile != NULL) {
            int32_t points;
            switch (missile->type) {
                default:
                    points = 100L;
                    break;
                case OBJ_MISSILE_BEETROOT:
                    points = 200L;
                    break;
                case OBJ_MISSILE_HAMMER:
                    points = 500L;
                    break;
            }
            give_points(missile, pid, points);
            missile->flags |= FLG_DESTROY;
        }
    }

    state.pswitch = 0L;
    pawn->values[VAL_PLAYER_FLASH] = pawn->values[VAL_PLAYER_STARMAN] = 0L;
    stop_track(TS_LEVEL);
    stop_track(TS_SWITCH);
    stop_track(TS_POWER);
    play_track(TS_FANFARE, (state.flags & GF_LOST) ? "WIN2" : "WIN", false);
}

static void kill_player(struct GameObject* pawn) {
    struct GameObject* dead = get_object(create_object(OBJ_PLAYER_DEAD, pawn->pos));
    if (dead != NULL) {
        // !!! CLIENT-SIDE !!!
        if (pawn->values[VAL_PLAYER_INDEX] == local_player && pawn->values[VAL_PLAYER_STARMAN] > 0L)
            stop_track(TS_POWER);
        // !!! CLIENT-SIDE !!!

        struct GamePlayer* player =
            get_player((PlayerID)(dead->values[VAL_PLAYER_INDEX] = pawn->values[VAL_PLAYER_INDEX]));
        if (player != NULL) {
            player->power = POW_SMALL;

            struct GameObject* kpawn = get_object(player->kevin.object);
            if (kpawn != NULL) {
                create_object(OBJ_EXPLODE, (fvec2){kpawn->pos[0], Fsub(kpawn->pos[1], FfInt(16L))});
                kpawn->flags |= FLG_DESTROY;
            }
            player->kevin.object = NULLOBJ;
        }

        Bool all_dead = true;
        if (state.sequence.type == SEQ_NONE && state.clock != 0L)
            for (size_t i = 0; i < MAX_PLAYERS; i++)
                if (state.players[i].lives > 0L) {
                    all_dead = false;
                    break;
                }
        if (all_dead) {
            state.sequence.type = SEQ_LOSE;
            state.sequence.time = 0L;
            dead->flags |= FLG_PLAYER_DEAD_LAST;
        }

        pawn->flags |= FLG_DESTROY;
    }
}

static Bool hit_player(struct GameObject* pawn) {
    if (state.sequence.type == SEQ_WIN || pawn->values[VAL_PLAYER_FLASH] > 0L || pawn->values[VAL_PLAYER_STARMAN] > 0L)
        return false;

    struct GamePlayer* player = get_player((PlayerID)(pawn->values[VAL_PLAYER_INDEX]));
    if (player != NULL) {
        if (player->power == POW_SMALL) {
            kill_player(pawn);
            return true;
        }
        player->power = (player->power == POW_BIG) ? POW_SMALL : POW_BIG;
    }

    pawn->values[VAL_PLAYER_POWER] = 3000L;
    pawn->values[VAL_PLAYER_FLASH] = 100L;
    play_sound_at_object(pawn, "WARP");
    return true;
}

static Bool bump_check(ObjectID self_id, ObjectID other_id) {
    struct GameObject* self = &(state.objects[self_id]);
    switch (self->type) {
        default:
            break;

        case OBJ_PLAYER: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            if (self->values[VAL_PLAYER_GROUND] <= 0L && self->values[VAL_Y_SPEED] > FxZero &&
                self->pos[1] < Fsub(other->pos[1], FfInt(10L))) {
                const struct GamePlayer* player = get_owner(self_id);
                self->values[VAL_Y_SPEED] = FfInt((player != NULL && (player->input & GI_JUMP)) ? -13L : -8L);
                other->values[VAL_Y_SPEED] = Fmax(other->values[VAL_Y_SPEED], FxZero);
                play_sound_at_object(other, "STOMP");
            } else if (Fabs(self->values[VAL_X_SPEED]) > Fabs(other->values[VAL_X_SPEED])) {
                if ((self->values[VAL_X_SPEED] > FxZero && self->pos[0] < other->pos[0]) ||
                    (self->values[VAL_X_SPEED] < FxZero && self->pos[0] > other->pos[0])) {
                    other->values[VAL_X_SPEED] = Fadd(other->values[VAL_X_SPEED], Fhalf(self->values[VAL_X_SPEED]));
                    self->values[VAL_X_SPEED] = -Fhalf(self->values[VAL_X_SPEED]);
                    if (Fabs(self->values[VAL_X_SPEED]) >= FfInt(2L))
                        play_sound_at_object(self, "BUMP");
                }
            }
            break;
        }

        case OBJ_KEVIN: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            if (other->values[VAL_PLAYER_INDEX] != self->values[VAL_KEVIN_PLAYER]) {
                hit_player(other);
            } else {
                kill_player(other);
                play_sound_at_object(self, "KEVINDIE");
            }
            break;
        }

        case OBJ_COIN:
        case OBJ_PSWITCH_COIN: {
            const struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (++(player->coins) >= 100L) {
                give_points(self, pid, -1L);
                player->coins -= 100L;
            }

            player->score += 200L;
            play_sound_at_object(self, "COIN");
            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_MUSHROOM: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (player->power == POW_SMALL) {
                other->values[VAL_PLAYER_POWER] = 3000L;
                player->power = POW_BIG;
            }
            give_points(self, pid, 1000L);

            play_sound_at_object(other, "GROW");
            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_MUSHROOM_1UP: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            give_points(self, get_owner_id(other_id), -1L);
            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_MUSHROOM_POISON: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER && other->values[VAL_PLAYER_STARMAN] <= 0L) {
                kill_player(other);
                create_object(OBJ_EXPLODE, (fvec2){self->pos[0], Fsub(self->pos[1], FfInt(15L))});
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_FIRE_FLOWER: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                other->values[VAL_PLAYER_POWER] = 3000L;
                player->power = POW_BIG;
            } else if (player->power != POW_FIRE) {
                other->values[VAL_PLAYER_POWER] = 4000L;
                player->power = POW_FIRE;
            }
            give_points(self, pid, 1000L);
            play_sound_at_object(other, "GROW");

            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_BEETROOT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                other->values[VAL_PLAYER_POWER] = 3000L;
                player->power = POW_BIG;
            } else if (player->power != POW_BEETROOT) {
                other->values[VAL_PLAYER_POWER] = 4000L;
                player->power = POW_BEETROOT;
            }
            give_points(self, pid, 1000L);
            play_sound_at_object(other, "GROW");

            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_LUI: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                other->values[VAL_PLAYER_POWER] = 3000L;
                player->power = POW_BIG;
            } else if (player->power != POW_LUI) {
                other->values[VAL_PLAYER_POWER] = 4000L;
                player->power = POW_LUI;
            }
            give_points(self, pid, 1000L);
            play_sound_at_object(other, "GROW");

            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_HAMMER_SUIT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            const PlayerID pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                other->values[VAL_PLAYER_POWER] = 3000L;
                player->power = POW_BIG;
            } else if (player->power != POW_HAMMER) {
                other->values[VAL_PLAYER_POWER] = 4000L;
                player->power = POW_HAMMER;
            }
            give_points(self, pid, 1000L);
            play_sound_at_object(other, "GROW");

            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_STARMAN: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            // !!! CLIENT-SIDE !!!
            if (other->values[VAL_PLAYER_INDEX] == local_player)
                play_track(TS_POWER, "STARMAN", true);
            // !!! CLIENT-SIDE !!!
            other->values[VAL_PLAYER_STARMAN] = 500L;
            play_sound_at_object(other, "GROW");

            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_CHECKPOINT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER || state.checkpoint >= self_id)
                break;

            const PlayerID pid = get_owner_id(other_id);
            give_points(self, pid, 1000L);

            struct GamePlayer* player = get_player(pid);
            if (player != NULL && self->values[VAL_CHECKPOINT_BOUNDS_X1] != self->values[VAL_CHECKPOINT_BOUNDS_X2] &&
                self->values[VAL_CHECKPOINT_BOUNDS_Y1] != self->values[VAL_CHECKPOINT_BOUNDS_Y2]) {
                player->bounds[0][0] = self->values[VAL_CHECKPOINT_BOUNDS_X1];
                player->bounds[0][1] = self->values[VAL_CHECKPOINT_BOUNDS_Y1];
                player->bounds[1][0] = self->values[VAL_CHECKPOINT_BOUNDS_X2];
                player->bounds[1][1] = self->values[VAL_CHECKPOINT_BOUNDS_Y2];
            }

            play_sound_at_object(self, "SPROUT");
            state.checkpoint = self_id;
            break;
        }

        case OBJ_ROTODISC: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            hit_player(other);
            break;
        }

        case OBJ_GOAL_MARK: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER || state.sequence.type == SEQ_WIN)
                break;

            if (other->values[VAL_PLAYER_GROUND] > 0L) {
                give_points(other, get_owner_id(other_id), 100L);
                win_player(other);
            }
            break;
        }

        case OBJ_GOAL_BAR: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER || state.sequence.type == SEQ_WIN)
                break;

            int32_t points;
            if (self->pos[1] < Fadd(self->values[VAL_GOAL_Y], FfInt(30L)))
                points = 10000L;
            else if (self->pos[1] < Fadd(self->values[VAL_GOAL_Y], FfInt(60L)))
                points = 5000L;
            else if (self->pos[1] < Fadd(self->values[VAL_GOAL_Y], FfInt(100L)))
                points = 2000L;
            else if (self->pos[1] < Fadd(self->values[VAL_GOAL_Y], FfInt(150L)))
                points = 1000L;
            else if (self->pos[1] < Fadd(self->values[VAL_GOAL_Y], FfInt(200L)))
                points = 500L;
            else
                points = 200L;
            give_points(self, get_owner_id(other_id), points);
            win_player(other);

            struct GameObject* bar = get_object(
                create_object(OBJ_GOAL_BAR_FLY, (fvec2){Fsub(self->pos[0], FfInt(2L)), Fadd(self->pos[1], FfInt(7L))})
            );
            if (bar != NULL) {
                const fix16_t dir = Fadd(0x00025B30, Fadd(-0x00003244, Fmul(FfInt(random() % 3), 0x00003244)));
                bar->values[VAL_X_SPEED] = Fmul(0x00050000, Fcos(dir));
                bar->values[VAL_Y_SPEED] = Fmul(0x00050000, -Fsin(dir));
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_HIDDEN_BLOCK: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;

            if (other->values[VAL_Y_SPEED] < FxZero &&
                Fsub(Fadd(other->pos[1], other->bbox[0][1]), other->values[VAL_Y_SPEED]) >
                    Fadd(self->pos[1], self->bbox[1][1])) {
                struct GameObject* block = get_object(create_object(OBJ_ITEM_BLOCK, self->pos));
                if (block != NULL) {
                    move_object(
                        other_id, (fvec2){other->pos[0], Fsub(Fadd(self->pos[1], self->bbox[1][1]), other->bbox[0][1])}
                    );
                    block->values[VAL_BLOCK_ITEM] = self->values[VAL_BLOCK_ITEM];
                    bump_block(block, other_id, false);
                    self->flags |= FLG_DESTROY;
                }
            }
            break;
        }

        case OBJ_PSWITCH: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER || other->values[VAL_Y_SPEED] <= FxZero ||
                other->pos[1] > Fadd(self->pos[1], FfInt(20L)) || self->values[VAL_PSWITCH] > 0L)
                break;

            other->values[VAL_Y_SPEED] = -FxOne;
            self->values[VAL_PSWITCH] = state.pswitch = 600L;

            ObjectID oid = state.live_objects;
            while (object_is_alive(oid)) {
                struct GameObject* object = &(state.objects[oid]);
                if (object->type == OBJ_COIN) {
                    create_object(OBJ_PSWITCH_BRICK, object->pos);
                    object->flags |= FLG_DESTROY;
                } else if (object->type == OBJ_BRICK_BLOCK) {
                    create_object(OBJ_PSWITCH_COIN, object->pos);
                    object->flags |= FLG_DESTROY;
                }
                oid = object->previous;
            }

            play_sound_at_object(self, "TOGGLE");
            play_track(TS_SWITCH, "PSWITCH", true);
            break;
        }

        case OBJ_THWOMP: {
            struct GameObject* other = &(state.objects[other_id]);
            switch (other->type) {
                default:
                    break;

                case OBJ_PLAYER: {
                    if (hit_player(other)) {
                        self->values[VAL_THWOMP_FRAME] = FxZero;
                        self->flags |= FLG_THWOMP_LAUGH;
                        play_sound_at_object(self, "THWOMP");
                    }
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    play_sound_at_object(other, "BUMP");
                    other->flags |= FLG_DESTROY;
                    break;
                }
            }
            break;
        }
    }

    return true;
}

static void bump_object(ObjectID bid) {
    struct GameObject* object = get_object(bid);
    if (object == NULL)
        return;

    fix16_t x1 = Fadd(object->pos[0], object->bbox[0][0]);
    fix16_t y1 = Fadd(object->pos[1], object->bbox[0][1]);
    fix16_t x2 = Fadd(object->pos[0], object->bbox[1][0]);
    fix16_t y2 = Fadd(object->pos[1], object->bbox[1][1]);

    int32_t bx1 = Fsub(x1, BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by1 = Fsub(y1, BLOCK_SIZE) / BLOCK_SIZE;
    int32_t bx2 = Fadd(x2, BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by2 = Fadd(y2, BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);
    for (int32_t by = by1; by <= by2; by++)
        for (int32_t bx = bx1; bx <= bx2; bx++) {
            ObjectID oid = state.blockmap[bx + (by * MAX_BLOCKS)];
            while (object_is_alive(oid)) {
                const struct GameObject* other = &(state.objects[oid]);
                const ObjectID next = other->previous_block;
                if (bid != oid && other->values[VAL_SPROUT] <= 0L && !(other->flags & FLG_DESTROY)) {
                    const fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                    const fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                    const fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                    const fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                    if (x1 < ox2 && x2 > ox1 && y1 < oy2 && y2 > oy1 && !bump_check(oid, bid))
                        return;
                }
                oid = next;
            }
        }
}

static PlayerFrames get_player_frame(const struct GameObject* object) {
    if (object->values[VAL_PLAYER_POWER] > 0L) {
        const struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_PLAYER_INDEX]));
        switch (player == NULL ? POW_SMALL : player->power) {
            case POW_SMALL:
            case POW_BIG: {
                switch ((object->values[VAL_PLAYER_POWER] / 100L) % 3) {
                    default:
                        return PF_GROW1;
                    case 1:
                        return PF_GROW2;
                    case 2:
                        return PF_GROW3;
                }
                break;
            }

            default: {
                switch ((object->values[VAL_PLAYER_POWER] / 100L) % 4) {
                    default:
                        return PF_GROW1;
                    case 1:
                        return PF_GROW2;
                    case 2:
                        return PF_GROW3;
                    case 3:
                        return PF_GROW4;
                }
                break;
            }
        }
    }

    if (object->values[VAL_PLAYER_GROUND] <= 0L) {
        if (object->flags & FLG_PLAYER_SWIM) {
            const int32_t frame = FtInt(object->values[VAL_PLAYER_FRAME]);
            switch (frame) {
                case 0:
                case 3:
                    return PF_SWIM1;
                case 1:
                case 4:
                    return PF_SWIM2;
                case 2:
                case 5:
                    return PF_SWIM3;
                default:
                    return (frame % 2L) ? PF_SWIM5 : PF_SWIM4;
            }
        } else
            return (object->values[VAL_Y_SPEED] < FxZero) ? PF_JUMP : PF_FALL;
    }

    if (object->flags & FLG_PLAYER_DUCK)
        return PF_DUCK;

    if (object->values[VAL_PLAYER_FIRE] > 0L)
        return PF_FIRE;

    if (object->values[VAL_X_SPEED] != FxZero)
        switch (FtInt(object->values[VAL_PLAYER_FRAME]) % 3) {
            default:
                return PF_WALK1;
            case 1:
                return PF_WALK2;
            case 2:
                return PF_WALK3;
        }

    return PF_IDLE;
}

static const char* get_player_texture(PlayerPowers power, PlayerFrames frame) {
    switch (power) {
        default:
        case POW_SMALL: {
            switch (frame) {
                default:
                    return "P_S_IDLE";

                case PF_WALK1:
                    return "P_S_WLKA";
                case PF_WALK2:
                    return "P_S_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_S_JUMP";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_S_SWMA";
                case PF_SWIM2:
                    return "P_S_SWMB";
                case PF_SWIM3:
                    return "P_S_SWMC";
                case PF_SWIM5:
                    return "P_S_SWMD";

                case PF_GROW2:
                    return "P_GROWA";
                case PF_GROW3:
                    return "P_B_WLKB";
            }
            break;
        }

        case POW_BIG: {
            switch (frame) {
                default:
                    return "P_B_IDLE";

                case PF_WALK1:
                    return "P_B_WLKA";
                case PF_WALK2:
                case PF_GROW1:
                    return "P_B_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_B_JUMP";

                case PF_DUCK:
                    return "P_B_DUCK";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_B_SWMA";
                case PF_SWIM2:
                    return "P_B_SWMB";
                case PF_SWIM3:
                    return "P_B_SWMC";
                case PF_SWIM5:
                    return "P_B_SWMD";

                case PF_GROW2:
                    return "P_GROWA";
                case PF_GROW3:
                    return "P_S_IDLE";
            }
            break;
        }

        case POW_FIRE: {
            switch (frame) {
                default:
                    return "P_F_IDLE";

                case PF_WALK1:
                    return "P_F_WLKA";
                case PF_WALK2:
                case PF_GROW3:
                    return "P_F_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_F_JUMP";

                case PF_DUCK:
                    return "P_F_DUCK";

                case PF_FIRE:
                    return "P_F_FIRE";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_F_SWMA";
                case PF_SWIM2:
                    return "P_F_SWMB";
                case PF_SWIM3:
                    return "P_F_SWMC";
                case PF_SWIM5:
                    return "P_F_SWMD";

                case PF_GROW1:
                    return "P_B_WLKB";
                case PF_GROW2:
                    return "P_GROWB";
                case PF_GROW4:
                    return "P_GROWC";
            }
            break;
        }

        case POW_BEETROOT: {
            switch (frame) {
                default:
                    return "P_P_IDLE";

                case PF_WALK1:
                    return "P_P_WLKA";
                case PF_WALK2:
                case PF_GROW3:
                    return "P_P_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_P_JUMP";

                case PF_DUCK:
                    return "P_P_DUCK";

                case PF_FIRE:
                    return "P_P_FIRE";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_P_SWMA";
                case PF_SWIM2:
                    return "P_P_SWMB";
                case PF_SWIM3:
                    return "P_P_SWMC";
                case PF_SWIM5:
                    return "P_P_SWMD";

                case PF_GROW1:
                    return "P_B_WLKB";
                case PF_GROW2:
                    return "P_GROWB";
                case PF_GROW4:
                    return "P_GROWC";
            }
            break;
        }

        case POW_LUI: {
            switch (frame) {
                default:
                    return "P_L_IDLE";

                case PF_WALK1:
                    return "P_L_WLKA";
                case PF_WALK2:
                case PF_GROW3:
                    return "P_L_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_L_JUMP";

                case PF_DUCK:
                    return "P_L_DUCK";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_L_SWMA";
                case PF_SWIM2:
                    return "P_L_SWMB";
                case PF_SWIM3:
                    return "P_L_SWMC";
                case PF_SWIM5:
                    return "P_L_SWMD";

                case PF_GROW1:
                    return "P_B_WLKB";
                case PF_GROW2:
                    return "P_GROWB";
                case PF_GROW4:
                    return "P_GROWC";
            }
            break;
        }

        case POW_HAMMER: {
            switch (frame) {
                default:
                    return "P_H_IDLE";

                case PF_WALK1:
                    return "P_H_WLKA";
                case PF_WALK2:
                case PF_GROW3:
                    return "P_H_WLKB";

                case PF_JUMP:
                case PF_FALL:
                    return "P_H_JUMP";

                case PF_DUCK:
                    return "P_H_DUCK";

                case PF_FIRE:
                    return "P_H_FIRE";

                case PF_SWIM1:
                case PF_SWIM4:
                    return "P_H_SWMA";
                case PF_SWIM2:
                    return "P_H_SWMB";
                case PF_SWIM3:
                    return "P_H_SWMC";
                case PF_SWIM5:
                    return "P_H_SWMD";

                case PF_GROW1:
                    return "P_B_WLKB";
                case PF_GROW2:
                    return "P_GROWB";
                case PF_GROW4:
                    return "P_GROWC";
            }
            break;
        }
    }
}

static Bool in_any_view(struct GameObject* object, fix16_t padding, Bool ignore_top) {
    const fix16_t sx1 = Fadd(object->pos[0], object->bbox[0][0]);
    const fix16_t sy1 = Fadd(object->pos[1], object->bbox[0][1]);
    const fix16_t sx2 = Fadd(object->pos[0], object->bbox[1][0]);
    const fix16_t sy2 = Fadd(object->pos[1], object->bbox[1][1]);

    const struct GameObject* scroller = get_object(state.autoscroll);
    if (scroller != NULL) {
        const fix16_t ox1 = Fsub(state.bounds[0][0], padding);
        const fix16_t oy1 = Fsub(state.bounds[0][1], padding);
        const fix16_t ox2 = Fadd(state.bounds[1][0], padding);
        const fix16_t oy2 = Fadd(state.bounds[1][1], padding);
        if (sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && (ignore_top || sy2 > oy1))
            return true;
    } else
        for (size_t i = 0; i < MAX_PLAYERS; i++) {
            const struct GamePlayer* player = &(state.players[i]);
            if (!(player->active) || !object_is_alive(player->object))
                continue;

            const struct GameObject* pawn = &(state.objects[player->object]);
            const fix16_t ox = Fclamp(
                pawn->pos[0], Fadd(player->bounds[0][0], F_HALF_SCREEN_WIDTH),
                Fsub(player->bounds[1][0], F_HALF_SCREEN_WIDTH)
            );
            const fix16_t oy = Fclamp(
                pawn->pos[1], Fadd(player->bounds[0][1], F_HALF_SCREEN_HEIGHT),
                Fsub(player->bounds[1][1], F_HALF_SCREEN_HEIGHT)
            );

            const fix16_t ox1 = Fsub(Fsub(ox, F_HALF_SCREEN_WIDTH), padding);
            const fix16_t oy1 = Fsub(Fsub(oy, F_HALF_SCREEN_HEIGHT), padding);
            const fix16_t ox2 = Fadd(Fadd(ox, F_HALF_SCREEN_WIDTH), padding);
            const fix16_t oy2 = Fadd(Fadd(oy, F_HALF_SCREEN_HEIGHT), padding);
            if (sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && (ignore_top || sy2 > oy1))
                return true;
        }

    return false;
}

static Bool in_player_view(struct GameObject* object, PlayerID pid, fix16_t padding, Bool ignore_top) {
    const struct GamePlayer* player = get_player(pid);
    if (player == NULL || !(player->active) || !object_is_alive(player->object))
        return false;

    const fix16_t sx1 = Fadd(object->pos[0], object->bbox[0][0]);
    const fix16_t sy1 = Fadd(object->pos[1], object->bbox[0][1]);
    const fix16_t sx2 = Fadd(object->pos[0], object->bbox[1][0]);
    const fix16_t sy2 = Fadd(object->pos[1], object->bbox[1][1]);

    const struct GameObject* scroller = get_object(state.autoscroll);
    if (scroller != NULL) {
        const fix16_t ox1 = Fsub(state.bounds[0][0], padding);
        const fix16_t oy1 = Fsub(state.bounds[0][1], padding);
        const fix16_t ox2 = Fadd(state.bounds[1][0], padding);
        const fix16_t oy2 = Fadd(state.bounds[1][1], padding);
        return sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && (ignore_top || sy2 > oy1);
    }

    const struct GameObject* pawn = &(state.objects[player->object]);
    const fix16_t ox = Fclamp(
        pawn->pos[0], Fadd(player->bounds[0][0], F_HALF_SCREEN_WIDTH), Fsub(player->bounds[1][0], F_HALF_SCREEN_WIDTH)
    );
    const fix16_t oy = Fclamp(
        pawn->pos[1], Fadd(player->bounds[0][1], F_HALF_SCREEN_HEIGHT), Fsub(player->bounds[1][1], F_HALF_SCREEN_HEIGHT)
    );

    const fix16_t ox1 = Fsub(Fsub(ox, F_HALF_SCREEN_WIDTH), padding);
    const fix16_t oy1 = Fsub(Fsub(oy, F_HALF_SCREEN_HEIGHT), padding);
    const fix16_t ox2 = Fadd(Fadd(ox, F_HALF_SCREEN_WIDTH), padding);
    const fix16_t oy2 = Fadd(Fadd(oy, F_HALF_SCREEN_HEIGHT), padding);

    return sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && (ignore_top || sy2 > oy1);
}

static Bool below_frame(struct GameObject* object) {
    return Fadd(object->pos[1], object->bbox[0][1]) > state.size[1];
}

/* ======

   ENGINE

   ====== */

void start_state(PlayerID num_players, PlayerID local, const char* level, GameFlags flags) {
    local_player = local;

    SDL_memset(&state, 0, sizeof(state));
    clear_tiles();
    state.live_objects = NULLOBJ;
    for (uint32_t i = 0L; i < BLOCKMAP_SIZE; i++)
        state.blockmap[i] = NULLOBJ;

    state.size[0] = state.bounds[1][0] = F_SCREEN_WIDTH;
    state.size[1] = state.bounds[1][1] = F_SCREEN_HEIGHT;

    state.spawn = state.checkpoint = state.autoscroll = NULLOBJ;
    state.water = FfInt(32767L);

    state.flags |= flags;
    state.clock = -1L;

    state.sequence.activator = -1L;

    //
    //
    //
    // DATA LOADER
    //
    //
    //
    load_texture("M_WATERA");
    load_texture("M_WATERB");
    load_texture("M_WATERC");
    load_texture("M_WATERD");
    load_texture("M_WATERE");
    load_texture("M_WATERF");
    load_texture("M_WATERG");
    load_texture("M_WATERH");

    load_font(FNT_HUD);

    load_sound("HURRY");
    load_sound("TICK");

    //
    //
    //
    // LEVEL LOADER
    //
    //
    //
    uint8_t* data = SDL_LoadFile(find_file(file_pattern("data/levels/%s.kla", level), NULL), NULL);
    if (data == NULL)
        FATAL("Failed to load level \"%s\": %s", level, SDL_GetError());
    const uint8_t* buf = data;

    // Header
    if (SDL_strncmp((const char*)buf, "Klawiatura", sizeof(char[10])) != 0)
        FATAL("Invalid level header");
    buf += sizeof(char[10]);
    buf += sizeof(uint8_t); // MAJOR_LEVEL_VERSION
    buf += sizeof(uint8_t); // MINOR_LEVEL_VERSION

    // Level
    char level_name[32];
    SDL_memcpy(level_name, buf, sizeof(char[32]));
    INFO("Level: %s (%s)", level, level_name);
    buf += sizeof(char[32]);

    buf += sizeof(char[8]); // HUD Texture
    buf += sizeof(char[8]); // Next Level

    char track[9];
    SDL_memcpy(track, buf, sizeof(char[8]));
    track[8] = '\0';
    load_track(track);
    buf += sizeof(char[8]);

    buf += sizeof(char[8]); // Secondary Track

    state.flags |= *((GameFlags*)buf);
    buf += sizeof(GameFlags);

    state.size[0] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);
    state.size[1] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);

    state.bounds[0][0] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);
    state.bounds[0][1] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);
    state.bounds[1][0] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);
    state.bounds[1][1] = *((fix16_t*)buf);
    buf += sizeof(fix16_t);

    state.clock = *((int32_t*)buf);
    buf += sizeof(int32_t);

    state.water = *((fix16_t*)buf);
    buf += sizeof(fix16_t);

    state.hazard = *((fix16_t*)buf);
    buf += sizeof(fix16_t);

    // Markers
    uint32_t num_markers = *((uint32_t*)buf);
    buf += sizeof(uint32_t);

    for (uint32_t i = 0; i < num_markers; i++) {
        // Skip editor def
        while (*buf != '\0')
            buf += sizeof(char);
        buf += sizeof(char);

        uint8_t marker_type = *((uint8_t*)buf);
        buf += sizeof(uint8_t);

        switch (marker_type) {
            default: {
                FATAL("Unknown marker type %u", marker_type);
                break;
            }

            case 1: { // Gradient
                char texture[9];
                SDL_memcpy(texture, buf, sizeof(char[8]));
                texture[8] = '\0';
                buf += sizeof(char[8]);

                GLfloat rect[2][2] = {
                    {*((GLfloat*)buf), *((GLfloat*)(buf + sizeof(GLfloat)))},
                    {*((GLfloat*)(buf + sizeof(GLfloat[2]))), *((GLfloat*)(buf + sizeof(GLfloat[3])))},
                };
                buf += sizeof(GLfloat[2][2]);

                GLfloat z = *((GLfloat*)buf);
                buf += sizeof(GLfloat);

                GLubyte color[4][4] = {
                    {*((GLubyte*)buf), *((GLubyte*)(buf + sizeof(GLubyte))), *((GLubyte*)(buf + sizeof(GLubyte[2]))),
                     *((GLubyte*)(buf + sizeof(GLubyte[3])))},

                    {*((GLubyte*)(buf + sizeof(GLubyte[4]))), *((GLubyte*)(buf + sizeof(GLubyte[5]))),
                     *((GLubyte*)(buf + sizeof(GLubyte[6]))), *((GLubyte*)(buf + sizeof(GLubyte[7])))},

                    {*((GLubyte*)(buf + sizeof(GLubyte[8]))), *((GLubyte*)(buf + sizeof(GLubyte[9]))),
                     *((GLubyte*)(buf + sizeof(GLubyte[10]))), *((GLubyte*)(buf + sizeof(GLubyte[11])))},

                    {*((GLubyte*)(buf + sizeof(GLubyte[12]))), *((GLubyte*)(buf + sizeof(GLubyte[13]))),
                     *((GLubyte*)(buf + sizeof(GLubyte[14]))), *((GLubyte*)(buf + sizeof(GLubyte[15])))}
                };
                buf += sizeof(GLubyte[4][4]);

                add_gradient(texture, rect, z, color);
                break;
            }

            case 2: { // Backdrop
                char texture[9];
                SDL_memcpy(texture, buf, sizeof(char[8]));
                texture[8] = '\0';
                buf += sizeof(char[8]);

                GLfloat pos[3] = {
                    *((GLfloat*)buf), *((GLfloat*)(buf + sizeof(GLfloat))), *((GLfloat*)(buf + sizeof(GLfloat[2])))
                };
                buf += sizeof(GLfloat[3]);

                buf += sizeof(GLfloat[2]); // Scale

                GLubyte color[4] = {
                    *((GLubyte*)buf), *((GLubyte*)(buf + sizeof(GLubyte))), *((GLubyte*)(buf + sizeof(GLubyte[2]))),
                    *((GLubyte*)(buf + sizeof(GLubyte[3])))
                };
                buf += sizeof(GLubyte[4]);

                add_backdrop(texture, pos, color);
                break;
            }

            case 3: {
                GameObjectType type = *((GameObjectType*)buf);
                buf += sizeof(GameObjectType);

                fvec2 pos = {*((fix16_t*)buf), *((fix16_t*)(buf + sizeof(fix16_t)))};
                buf += sizeof(fvec2);

                struct GameObject* object = get_object(create_object(type, pos));
                if (object == NULL)
                    FATAL("Failed to create object type %u", type);

                object->depth = *((fix16_t*)buf);
                buf += sizeof(fix16_t);

                fvec2 scale = {*((fix16_t*)buf), *((fix16_t*)(buf + sizeof(fix16_t)))};
                buf += sizeof(fvec2);
                object->bbox[0][0] = Fmul(object->bbox[0][0], scale[0]);
                object->bbox[0][1] = Fmul(object->bbox[0][1], scale[1]);
                object->bbox[1][0] = Fmul(object->bbox[1][0], scale[0]);
                object->bbox[1][1] = Fmul(object->bbox[1][1], scale[1]);

                uint8_t num_values = *((uint8_t*)buf);
                buf += sizeof(uint8_t);

                for (uint8_t j = 0; j < num_values; j++) {
                    uint8_t index = *((uint8_t*)buf);
                    buf += sizeof(uint8_t);
                    object->values[index] = *((fix16_t*)buf);
                    buf += sizeof(fix16_t);
                }

                object->flags |= *((ObjectFlags*)buf);
                buf += sizeof(ObjectFlags);

                break;
            }
        }
    }

    SDL_free(data);

    //
    //
    //
    // PLAYER START
    //
    //
    //
    for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->active = i < num_players;

        player->object = NULLOBJ;
        player->kevin.object = NULLOBJ;
        if (player->active) {
            player->lives = 4L;
            respawn_player(i);
        }
    }

    play_track(TS_LEVEL, track, true);
}

uint32_t check_state() {
    uint32_t checksum = 0;
    const uint8_t* data = (uint8_t*)(&state);
    for (size_t i = 0; i < sizeof(struct GameState); i++)
        checksum += data[i];
    return checksum;
}

void save_state(struct GameState* to) {
    SDL_memcpy(to, &state, sizeof(struct GameState));
}

void load_state(const struct GameState* from) {
    SDL_memcpy(&state, from, sizeof(struct GameState));
}

void dump_state() {
    INFO("\n[PLAYERS]");
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        const struct GamePlayer* player = &(state.players[i]);
        if (!(player->active))
            continue;

        INFO("  Player %zu:", i);
        INFO("      Input: %u -> %u", player->last_input, player->last_input);
        INFO("      Object: %i", player->object);
        INFO(
            "      Bounds: (%.2f, %.2f) x (%.2f, %.2f)", FtDouble(player->bounds[0][0]), FtDouble(player->bounds[0][1]),
            FtDouble(player->bounds[1][0]), FtDouble(player->bounds[1][1])
        );
        INFO("      Lives: %i", player->lives);
        INFO("      Coins: %i", player->coins);
        INFO("      Score: %i", player->score);
        INFO("      Power: %i", player->power);
        INFO("");
    }

    INFO("\n[LEVEL]");
    INFO("  Flags: %u", state.flags);
    INFO("  Size: %.2f x %.2f", FtDouble(state.size[0]), FtDouble(state.size[1]));
    INFO(
        "   Bounds: (%.2f, %.2f) x (%.2f, %.2f)", FtDouble(state.bounds[0][0]), FtDouble(state.bounds[0][1]),
        FtDouble(state.bounds[1][0]), FtDouble(state.bounds[1][1])
    );
    INFO("  Time: %zu", state.time);
    INFO("  Seed: %u", state.seed);
    INFO("  Clock: %u", state.clock);
    INFO("  Spawn: %u", state.spawn);
    INFO("  Checkpoint: %u", state.checkpoint);
    INFO("  Autoscroll: %u", state.autoscroll);
    INFO("  Water Y: %.2f", FtDouble(state.water));
    INFO("  Hazard Y: %.2f", FtDouble(state.hazard));

    INFO("\n[SEQUENCE]");
    INFO("  Type: %u", state.sequence.type);
    INFO("  Time: %u", state.sequence.time);
    INFO("  Activator: %u", state.sequence.activator);

    INFO("\n[OBJECTS]");
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        const struct GameObject* object = &(state.objects[oid]);

        INFO("  Object %i:", oid);
        INFO("      Type: %i", object->type);
        INFO("      Block: %i", object->block);
        INFO("      Position: (%.2f, %.2f)", FtDouble(object->pos[0]), FtDouble(object->pos[1]));
        INFO(
            "      Hitbox: (%.2f, %.2f) x (%.2f, %.2f)", FtDouble(object->bbox[0][0]), FtDouble(object->bbox[0][1]),
            FtDouble(object->bbox[1][0]), FtDouble(object->bbox[1][1])
        );
        INFO("      Depth: %i", object->depth);

        INFO("      Non-Zero Values:");
        for (size_t i = 0; i < MAX_VALUES; i++)
            if (object->values[i] != 0L)
                INFO("          Value %zu: %i", i, object->values[i]);

        INFO("      Flags: %u", object->flags);
        INFO("");

        oid = object->previous;
    }
    INFO("  Last live object: %i", state.live_objects);
    INFO("  Next object ID: %i", state.next_object);

    // TODO: Dump blockmap
}

void tick_state(GameInput inputs[MAX_PLAYERS]) {
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->last_input = player->input;
        player->input = inputs[i];
    }

    const struct GameObject* autoscroll = get_object(state.autoscroll);
    if (autoscroll != NULL) {
        state.bounds[0][0] = Fclamp(autoscroll->pos[0], FxZero, Fsub(state.size[0], F_SCREEN_WIDTH));
        state.bounds[0][1] = Fclamp(autoscroll->pos[1], FxZero, Fsub(state.size[1], F_SCREEN_HEIGHT));
        state.bounds[1][0] = Fclamp(Fadd(autoscroll->pos[0], F_SCREEN_WIDTH), F_SCREEN_WIDTH, state.size[0]);
        state.bounds[1][1] = Fclamp(Fadd(autoscroll->pos[1], F_SCREEN_HEIGHT), F_SCREEN_HEIGHT, state.size[1]);
    }

    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        struct GameObject* object = &(state.objects[oid]);

        if (object->values[VAL_SPROUT] > 0L)
            --(object->values[VAL_SPROUT]);
        else
            switch (object->type) {
                default:
                    break;

                case OBJ_PLAYER: {
                    struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_PLAYER_INDEX]));
                    if (player == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    if (object->flags & FLG_PLAYER_RESPAWN) {
                        if (touching_solid(HITBOX(object))) {
                            move_object(
                                oid, (fvec2){Flerp(state.bounds[0][0], state.bounds[1][0], FxHalf),
                                             Fadd(object->pos[1], FxOne)}
                            );
                            break;
                        } else {
                            object->flags &= ~FLG_PLAYER_RESPAWN;
                        }
                    }

                    if (state.sequence.type == SEQ_WIN) {
                        player->input = GI_NONE;
                        object->values[VAL_X_SPEED] = 0x00028000;
                    }

                    const Bool cant_run = !(player->input & GI_RUN) || object->pos[1] >= state.water;
                    const Bool jumped = (player->input & GI_JUMP) && !(player->last_input & GI_JUMP);

                    if ((player->input & GI_RIGHT) && object->values[VAL_X_TOUCH] <= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] >= FxZero &&
                        ((cant_run && object->values[VAL_X_SPEED] < 0x00046000) ||
                         (!cant_run && object->values[VAL_X_SPEED] < 0x00078000)))
                        object->values[VAL_X_SPEED] = Fadd(object->values[VAL_X_SPEED], 0x00002000);
                    if ((player->input & GI_LEFT) && object->values[VAL_X_TOUCH] >= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] <= FxZero &&
                        ((cant_run && object->values[VAL_X_SPEED] > -0x00046000) ||
                         (!cant_run && object->values[VAL_X_SPEED] > -0x00078000)))
                        object->values[VAL_X_SPEED] = Fsub(object->values[VAL_X_SPEED], 0x00002000);

                    if (!(player->input & GI_RIGHT) && object->values[VAL_X_SPEED] > FxZero)
                        object->values[VAL_X_SPEED] = Fmax(Fsub(object->values[VAL_X_SPEED], 0x00002000), FxZero);
                    if (!(player->input & GI_LEFT) && object->values[VAL_X_SPEED] < FxZero)
                        object->values[VAL_X_SPEED] = Fmin(Fadd(object->values[VAL_X_SPEED], 0x00002000), FxZero);

                    if ((player->input & GI_RIGHT) && object->values[VAL_X_TOUCH] <= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] < FxZero)
                        object->values[VAL_X_SPEED] = Fadd(object->values[VAL_X_SPEED], 0x00006000);
                    if ((player->input & GI_LEFT) && object->values[VAL_X_TOUCH] >= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] > FxZero)
                        object->values[VAL_X_SPEED] = Fsub(object->values[VAL_X_SPEED], 0x00006000);

                    if ((player->input & GI_RIGHT) && object->values[VAL_X_TOUCH] <= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] < FxOne &&
                        object->values[VAL_X_SPEED] >= FxZero)
                        object->values[VAL_X_SPEED] = Fadd(object->values[VAL_X_SPEED], FxOne);
                    if ((player->input & GI_LEFT) && object->values[VAL_X_TOUCH] >= 0L &&
                        !(object->flags & FLG_PLAYER_DUCK) && object->values[VAL_X_SPEED] > -FxOne &&
                        object->values[VAL_X_SPEED] <= FxZero)
                        object->values[VAL_X_SPEED] = Fsub(object->values[VAL_X_SPEED], FxOne);

                    if (cant_run && Fabs(object->values[VAL_X_SPEED]) > 0x00046000)
                        object->values[VAL_X_SPEED] = Fsub(
                            object->values[VAL_X_SPEED],
                            object->values[VAL_X_SPEED] >= FxZero ? 0x00002000 : -0x00002000
                        );

                    if ((player->input & GI_DOWN) && !(player->input & GI_LEFT) && !(player->input & GI_RIGHT) &&
                        object->values[VAL_PLAYER_GROUND] > 0L && player->power != POW_SMALL)
                        object->flags |= FLG_PLAYER_DUCK;
                    if (object->flags & FLG_PLAYER_DUCK) {
                        if (!(player->input & GI_DOWN) || player->power == POW_SMALL)
                            object->flags &= ~FLG_PLAYER_DUCK;
                        else if (object->values[VAL_X_SPEED] > FxZero)
                            object->values[VAL_X_SPEED] = Fmax(Fsub(object->values[VAL_X_SPEED], 0x00006000), FxZero);
                        else if (object->values[VAL_X_SPEED] < FxZero)
                            object->values[VAL_X_SPEED] = Fmin(Fadd(object->values[VAL_X_SPEED], 0x00006000), FxZero);
                    }

                    if ((player->input & GI_JUMP) && object->values[VAL_Y_SPEED] < FxZero &&
                        object->pos[1] < state.water && !(player->input & GI_DOWN))
                        object->values[VAL_Y_SPEED] = Fsub(
                            object->values[VAL_Y_SPEED],
                            (player->power == POW_LUI)
                                ? 0x0000999A
                                : ((Fabs(object->values[VAL_X_SPEED]) < 0x0000A000) ? 0x00006666 : FxHalf)
                        );

                    if (jumped) {
                        object->values[VAL_PLAYER_SPRING] = 7L;
                        if (object->pos[1] >= state.water && !(player->input & GI_DOWN)) {
                            object->values[VAL_Y_TOUCH] = 0L;
                            object->values[VAL_Y_SPEED] = FfInt(
                                (Fadd(object->pos[1], object->bbox[0][1]) > Fadd(state.water, FfInt(16L)) ||
                                 Fadd(object->pos[1], object->bbox[1][1]) < state.water)
                                    ? -3L
                                    : -9L
                            );
                            object->values[VAL_PLAYER_GROUND] = 0L;
                            object->values[VAL_PLAYER_FRAME] = FxZero;
                            play_sound_at_object(object, "SWIM");
                        }
                    }

                    if (object->pos[1] >= state.water) {
                        if (!(object->flags & FLG_PLAYER_SWIM)) {
                            if (Fabs(object->values[VAL_X_SPEED]) >= FxHalf)
                                object->values[VAL_X_SPEED] = Fsub(
                                    object->values[VAL_X_SPEED],
                                    object->values[VAL_X_SPEED] >= FxZero ? FxHalf : -FxHalf
                                );
                            object->values[VAL_PLAYER_FRAME] = FxZero;
                            if (object->pos[1] < Fadd(state.water, FfInt(11L)))
                                create_object(OBJ_WATER_SPLASH, object->pos);
                            object->flags |= FLG_PLAYER_SWIM;
                        }
                    } else if (object->flags & FLG_PLAYER_SWIM) {
                        object->values[VAL_PLAYER_FRAME] = FxZero;
                        object->flags &= ~FLG_PLAYER_SWIM;
                    }

                    if (object->pos[1] < state.water) {
                        if ((jumped || ((player->input & GI_JUMP) && object->flags & FLG_PLAYER_JUMP)) &&
                            !(player->input & GI_DOWN) && object->values[VAL_PLAYER_GROUND] > 0L) {
                            object->values[VAL_PLAYER_GROUND] = 0L;
                            object->values[VAL_Y_SPEED] = FfInt(-13L);
                            object->flags &= ~FLG_PLAYER_JUMP;
                            play_sound_at_object(object, "JUMP");
                        }
                        if (!(player->input & GI_JUMP) && !(player->input & GI_DOWN) &&
                            object->values[VAL_PLAYER_GROUND] > 0L && (object->flags & FLG_PLAYER_JUMP))
                            object->flags &= ~FLG_PLAYER_JUMP;
                    }
                    if (object->pos[1] > state.water && (state.time % 5L) == 0L)
                        if ((random() % 10L) == 5L)
                            create_object(
                                OBJ_BUBBLE, (fvec2){object->pos[0],
                                                    Fsub(object->pos[1], FfInt(player->power == POW_SMALL ? 18L : 39L))}
                            );

                    if (jumped && object->values[VAL_PLAYER_GROUND] <= 0L && object->values[VAL_Y_SPEED] > FxZero)
                        object->flags |= FLG_PLAYER_JUMP;

                    object->bbox[0][1] =
                        (player->power == POW_SMALL || (object->flags & FLG_PLAYER_DUCK)) ? FfInt(-25L) : FfInt(-51L);

                    if (object->values[VAL_PLAYER_GROUND] > 0L)
                        --(object->values[VAL_PLAYER_GROUND]);

                    if ((object->flags & FLG_PLAYER_ASCEND)) {
                        move_object(oid, POS_SPEED(object));
                        if (object->values[VAL_Y_SPEED] >= FxZero)
                            object->flags &= ~FLG_PLAYER_ASCEND;
                    } else {
                        displace_object(oid, FfInt(10L), true);
                        if (object->values[VAL_Y_TOUCH] > 0L)
                            object->values[VAL_PLAYER_GROUND] = 2L;

                        const struct GameObject* autoscroll = get_object(state.autoscroll);
                        if (autoscroll != NULL) {
                            if (state.sequence.type != SEQ_WIN) {
                                if (Fadd(object->pos[0], object->bbox[0][0]) < state.bounds[0][0]) {
                                    move_object(
                                        oid, (fvec2){Fsub(state.bounds[0][0], object->bbox[0][0]), object->pos[1]}
                                    );
                                    object->values[VAL_X_SPEED] = Fmax(object->values[VAL_X_SPEED], FxZero);
                                    object->values[VAL_X_TOUCH] = -1L;

                                    if (touching_solid(HITBOX(object)))
                                        kill_player(object);
                                }
                                if (Fadd(object->pos[0], object->bbox[1][0]) > state.bounds[1][0]) {
                                    move_object(
                                        oid, (fvec2){Fsub(state.bounds[1][0], object->bbox[1][0]), object->pos[1]}
                                    );
                                    object->values[VAL_X_SPEED] = Fmin(object->values[VAL_X_SPEED], FxZero);
                                    object->values[VAL_X_TOUCH] = 1L;

                                    if (touching_solid(HITBOX(object)))
                                        kill_player(object);
                                }
                            }

                            if ((autoscroll->flags & FLG_SCROLL_TANKS) &&
                                (Fadd(object->pos[1], object->bbox[1][1]) >= Fsub(state.bounds[1][1], FfInt(64L)) ||
                                 object->values[VAL_PLAYER_GROUND] <= 0L) &&
                                !touching_solid(HITBOX_ADD(object, autoscroll->values[VAL_X_SPEED], FxZero)))
                                move_object(oid, POS_ADD(object, autoscroll->values[VAL_X_SPEED], FxZero));
                        }
                    }

                    if (object->values[VAL_Y_TOUCH] <= 0L) {
                        const Bool carried = object->flags & FLG_CARRIED;
                        if (object->pos[1] >= state.water) {
                            if (object->values[VAL_Y_SPEED] > FfInt(3L) && !carried) {
                                object->values[VAL_Y_SPEED] = Fmin(Fsub(object->values[VAL_Y_SPEED], FxOne), FfInt(3L));
                            } else if (object->values[VAL_Y_SPEED] < FfInt(3L) || carried) {
                                object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x0000199A);
                                object->flags &= ~FLG_CARRIED;
                            }
                        } else if (object->values[VAL_Y_SPEED] > FfInt(10L) && !carried) {
                            object->values[VAL_Y_SPEED] = Fmin(Fsub(object->values[VAL_Y_SPEED], FxOne), FfInt(10L));
                        } else if (object->values[VAL_Y_SPEED] < FfInt(10L) || carried) {
                            object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], FxOne);
                            object->flags &= ~FLG_CARRIED;
                        }
                    }

                    // Animation
                    if (object->values[VAL_X_SPEED] > FxZero &&
                        (player->input & GI_RIGHT || state.sequence.type == SEQ_WIN))
                        object->flags &= ~FLG_X_FLIP;
                    else if (object->values[VAL_X_SPEED] < FxZero &&
                             (player->input & GI_LEFT || state.sequence.type == SEQ_WIN))
                        object->flags |= FLG_X_FLIP;

                    if (object->values[VAL_PLAYER_POWER] > 0L)
                        object->values[VAL_PLAYER_POWER] -= 91L;
                    if (object->values[VAL_PLAYER_FIRE] > 0L)
                        --(object->values[VAL_PLAYER_FIRE]);

                    object->values[VAL_PLAYER_FRAME] =
                        (object->values[VAL_PLAYER_GROUND] > 0L)
                            ? (object->values[VAL_X_SPEED] != FxZero
                                   ? Fadd(
                                         object->values[VAL_PLAYER_FRAME],
                                         Fclamp(Fmul(Fabs(object->values[VAL_X_SPEED]), 0x0000147B), 0x000023D7, FxHalf)
                                     )
                                   : FxZero)
                            : ((object->flags & FLG_PLAYER_SWIM)
                                   ? Fadd(
                                         object->values[VAL_PLAYER_FRAME],
                                         Fclamp(
                                             Fmul(Fabs(object->values[VAL_X_SPEED]), 0x0000147B), 0x000023D7, 0x00003EB8
                                         )
                                     )
                                   : FxZero);

                    switch (player->power) {
                        default:
                            break;

                        case POW_FIRE:
                        case POW_BEETROOT:
                        case POW_HAMMER: {
                            if ((player->input & GI_FIRE) && !(player->last_input & GI_FIRE) &&
                                !(object->flags & FLG_PLAYER_DUCK))
                                for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                                    if (!object_is_alive((ObjectID)(object->values[i]))) {
                                        GameObjectType mtype;
                                        switch (player->power) {
                                            default:
                                                mtype = OBJ_NULL;
                                                break;

                                            case POW_FIRE:
                                                mtype = OBJ_MISSILE_FIREBALL;
                                                break;

                                            case POW_BEETROOT: {
                                                int32_t beetroots = 0L;
                                                for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END;
                                                     i++)
                                                    if (object_is_alive((ObjectID)(object->values[i])))
                                                        ++beetroots;
                                                mtype = (beetroots >= 6L) ? OBJ_NULL : OBJ_MISSILE_BEETROOT;
                                                break;
                                            }

                                            case POW_HAMMER:
                                                mtype = OBJ_MISSILE_HAMMER;
                                                break;
                                        }

                                        const ObjectID mid = create_object(
                                            mtype,
                                            (fvec2){
                                                Fadd(object->pos[0], FfInt((object->flags & FLG_X_FLIP) ? -5L : 5L)),
                                                Fsub(object->pos[1], FfInt(29L))
                                            }
                                        );
                                        if (object_is_alive(mid)) {
                                            object->values[i] = mid;
                                            struct GameObject* missile = &(state.objects[mid]);
                                            missile->values[VAL_MISSILE_OWNER] = oid;
                                            switch (mtype) {
                                                default:
                                                    break;

                                                case OBJ_MISSILE_FIREBALL: {
                                                    missile->flags |= object->flags & FLG_X_FLIP;
                                                    missile->values[VAL_X_SPEED] =
                                                        (object->flags & FLG_X_FLIP) ? -0x00082000 : 0x00082000;
                                                    break;
                                                }

                                                case OBJ_MISSILE_BEETROOT: {
                                                    missile->values[VAL_X_SPEED] =
                                                        (object->flags & FLG_X_FLIP) ? -0x00022000 : 0x00022000;
                                                    missile->values[VAL_Y_SPEED] = FfInt(-5);
                                                    break;
                                                }

                                                case OBJ_MISSILE_HAMMER: {
                                                    missile->flags |= object->flags & FLG_X_FLIP;
                                                    missile->values[VAL_X_SPEED] = Fadd(
                                                        (object->flags & FLG_X_FLIP) ? -0x0002A3D7 : 0x0002A3D7,
                                                        object->values[VAL_X_SPEED]
                                                    );
                                                    missile->values[VAL_Y_SPEED] =
                                                        Fadd(FfInt(-7), Fmin(0, Fhalf(object->values[VAL_Y_SPEED])));
                                                    break;
                                                }
                                            }

                                            if (object->values[VAL_PLAYER_GROUND] > 0L)
                                                object->values[VAL_PLAYER_FIRE] = 2L;
                                            play_sound_at_object(object, "FIRE");
                                        }
                                        break;
                                    }
                            break;
                        }

                        case POW_LUI: {
                            if (object->values[VAL_PLAYER_GROUND] <= 0L) {
                                const ObjectID eid = create_object(OBJ_PLAYER_EFFECT, object->pos);
                                struct GameObject* effect = get_object(eid);
                                if (effect != NULL) {
                                    interp[eid].from[0] = interp[oid].pos[0];
                                    interp[eid].from[1] = interp[oid].pos[1];
                                    effect->flags |= object->flags;
                                    effect->values[VAL_PLAYER_EFFECT_POWER] = player->power;
                                    effect->values[VAL_PLAYER_EFFECT_FRAME] = get_player_frame(object);
                                }
                            }
                            break;
                        }
                    }

                    if (object->values[VAL_PLAYER_FLASH] > 0L)
                        --(object->values[VAL_PLAYER_FLASH]);
                    if (object->values[VAL_PLAYER_STARMAN] > 0L) {
                        const int32_t starman = --(object->values[VAL_PLAYER_STARMAN]);
                        if (starman == 100L)
                            play_sound_at_object(object, "STARMAN");
                        // !!! CLIENT-SIDE !!!
                        if (starman <= 0L && object->values[VAL_PLAYER_INDEX] == local_player)
                            stop_track(TS_POWER);
                        // !!! CLIENT-SIDE !!!
                    }

                    if (object->pos[1] >
                        Fadd(
                            object_is_alive(state.autoscroll) ? state.bounds[1][1] : player->bounds[1][1], FfInt(64L)
                        )) {
                        kill_player(object);
                        break;
                    }

                    if ((state.flags & GF_KEVIN) && state.sequence.type == SEQ_NONE &&
                        ((object->values[VAL_PLAYER_KEVIN] <= 0L &&
                          Fabs(Fadd(
                              Fsub(object->pos[0], object->values[VAL_PLAYER_KEVIN_X]),
                              Fsub(object->pos[1], object->values[VAL_PLAYER_KEVIN_Y])
                          )) > FxOne) ||
                         object->values[VAL_PLAYER_KEVIN] > 0L)) {
                        struct Kevin* kevin = &(player->kevin);
                        SDL_memmove(kevin->frames, kevin->frames + 1L, (KEVIN_DELAY - 1L) * sizeof(struct KevinFrame));

                        if (object->values[VAL_PLAYER_KEVIN] < KEVIN_DELAY)
                            if (++(object->values[VAL_PLAYER_KEVIN]) >= KEVIN_DELAY) {
                                struct GameObject* kpawn =
                                    get_object(kevin->object = create_object(OBJ_KEVIN, kevin->frames[0].pos));
                                if (kpawn != NULL) {
                                    kpawn->values[VAL_KEVIN_PLAYER] = object->values[VAL_PLAYER_INDEX];
                                    play_sound_at_object(kpawn, "KEVINACT");
                                }
                            }

                        struct KevinFrame* kframe = &(kevin->frames[KEVIN_DELAY - 1L]);
                        kframe->pos[0] = object->pos[0];
                        kframe->pos[1] = object->pos[1];
                        kframe->flipped = object->flags & FLG_X_FLIP;
                        kframe->power = player->power;
                        kframe->frame = get_player_frame(object);
                    }

                    bump_object(oid);
                    break;
                }

                case OBJ_PLAYER_DEAD: {
                    const PlayerID pid = (PlayerID)(object->values[VAL_PLAYER_INDEX]);
                    struct GamePlayer* player = get_player(pid);
                    if (player == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    if (object->values[VAL_PLAYER_FRAME] >= 25L) {
                        object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                        move_object(oid, POS_SPEED(object));
                    }

                    switch ((object->values[VAL_PLAYER_FRAME])++) {
                        default:
                            break;

                        case 0: {
                            if (object->flags & FLG_PLAYER_DEAD_LAST) {
                                stop_track(TS_LEVEL);
                                stop_track(TS_SWITCH);
                                stop_track(TS_POWER);
                                play_track(TS_FANFARE, (state.flags & GF_LOST) ? "LOSE2" : "LOSE", false);
                                if (state.flags & GF_HARDCORE)
                                    play_sound("HARDCORE");
                            } else {
                                play_sound_at_object(object, (player->lives > 0L) ? "LOSE" : "DEAD");
                            }
                            break;
                        }

                        case 25: {
                            object->values[VAL_Y_SPEED] = FfInt(-10L);
                            break;
                        }

                        case 150: {
                            struct GameObject* pawn = get_object(respawn_player(pid));
                            if (pawn != NULL) {
                                play_sound_at_object(pawn, "RESPAWN");
                                pawn->values[VAL_PLAYER_FLASH] = 100L;
                                --(player->lives);
                                object->flags |= FLG_DESTROY;
                            }
                            break;
                        }

                        case 210: {
                            if ((object->flags & FLG_PLAYER_DEAD_LAST) || state.clock == 0L) {
                                state.sequence.type = SEQ_LOSE;
                                state.sequence.time = 1L;
                                play_track(TS_FANFARE, "GAMEOVER", false);
                            }

                            object->flags |= FLG_DESTROY;
                            break;
                        }
                    }

                    break;
                }

                case OBJ_PLAYER_EFFECT: {
                    object->depth = Fadd(object->depth, 0x00000FFF);

                    object->values[VAL_PLAYER_EFFECT_ALPHA] = Fsub(object->values[VAL_PLAYER_EFFECT_ALPHA], 0x0009F600);
                    if (object->values[VAL_PLAYER_EFFECT_ALPHA] <= FxZero)
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_KEVIN: {
                    const struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_KEVIN_PLAYER]));
                    if (player == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    const struct KevinFrame* kframe = &(player->kevin.frames[0]);
                    move_object(oid, kframe->pos);
                    if (kframe->flipped)
                        object->flags |= FLG_X_FLIP;
                    else
                        object->flags &= ~FLG_X_FLIP;

                    object->values[VAL_KEVIN_X_JITTER] = random() % 5L;
                    object->values[VAL_KEVIN_X_JITTER] -= random() % 5L;
                    object->values[VAL_KEVIN_Y_JITTER] = random() % 5L;
                    object->values[VAL_KEVIN_Y_JITTER] -= random() % 5L;
                    break;
                }

                case OBJ_LUI: {
                    if (object->values[VAL_LUI_BOUNCE] > 0L) {
                        object->values[VAL_LUI_BOUNCE] += 62L;
                        if (object->values[VAL_LUI_BOUNCE] >= 600L)
                            object->values[VAL_LUI_BOUNCE] = 0L;
                    }

                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00003333);

                    displace_object(oid, FxZero, false);
                    if (object->values[VAL_Y_TOUCH] > 0L) {
                        object->values[VAL_Y_SPEED] = FfInt(-7L);
                        object->values[VAL_LUI_BOUNCE] = 1L;
                        play_sound_at_object(object, "KICK");
                    }

                    break;
                }

                case OBJ_POINTS: {
                    const int32_t time = (object->values[VAL_POINTS_TIME])++;
                    if (time < 35L)
                        move_object(oid, (fvec2){object->pos[0], Fsub(object->pos[1], FxOne)});

                    if ((object->values[VAL_POINTS] >= 10000L && time >= 300L) ||
                        (object->values[VAL_POINTS] >= 1000L && time >= 200L) ||
                        (object->values[VAL_POINTS] < 1000L && time >= 100L))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    object->values[VAL_MISSILE_ANGLE] = Fadd(
                        object->values[VAL_MISSILE_ANGLE],
                        object->values[VAL_X_SPEED] < FxZero ? -0x00003244 : 0x00003244
                    );
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);

                    displace_object(oid, FfInt(10L), false);
                    if (object->values[VAL_X_TOUCH] != 0L)
                        object->flags |= FLG_DESTROY;
                    else
                        bump_object(oid);
                    if (object->flags & FLG_DESTROY) {
                        create_object(OBJ_EXPLODE, object->pos);
                        break;
                    }

                    if (object->values[VAL_Y_TOUCH] > 0L)
                        object->values[VAL_Y_SPEED] = FfInt(-5L);

                    const PlayerID player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
                    if ((player >= 0L && !in_player_view(object, player, FxZero, true)) ||
                        (player < 0L && !in_any_view(object, FxZero, true)))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MISSILE_HAMMER: {
                    object->values[VAL_MISSILE_ANGLE] = Fadd(
                        object->values[VAL_MISSILE_ANGLE],
                        object->values[VAL_X_SPEED] < FxZero ? -0x00001922 : 0x00001922
                    );
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00004925);

                    move_object(oid, POS_SPEED(object));

                    const PlayerID player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
                    if ((player >= 0L && !in_player_view(object, player, FxZero, true)) ||
                        (player < 0L && !in_any_view(object, FxZero, true)))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MISSILE_BEETROOT: {
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                    move_object(oid, POS_SPEED(object));

                    if (object->values[VAL_MISSILE_HITS] > 0L) {
                        if (object->values[VAL_MISSILE_HIT] > 0L)
                            --(object->values[VAL_MISSILE_HIT]);

                        struct BlockList list = {0};
                        list_block_at(
                            &list, (fvec2[2]){{
                                                  Fadd(object->pos[0], object->bbox[0][0]),
                                                  Fadd(object->pos[1], object->bbox[0][1]),
                                              },
                                              {
                                                  Fadd(object->pos[0], object->bbox[1][0]),
                                                  Fadd(object->pos[1], object->bbox[1][1]),
                                              }}
                        );

                        uint8_t hit = 0L;
                        for (size_t i = 0; i < list.num_objects; i++) {
                            const ObjectID bid = list.objects[i];
                            if (bid == oid)
                                continue;

                            if (is_solid(bid, false, true)) {
                                if (object->values[VAL_MISSILE_HIT] > 0L) {
                                    ++(object->values[VAL_MISSILE_HIT]);
                                } else {
                                    struct GameObject* bumped = &(state.objects[bid]);
                                    switch (bumped->type) {
                                        default:
                                            break;
                                        case OBJ_ITEM_BLOCK:
                                        case OBJ_BRICK_BLOCK:
                                        case OBJ_BRICK_BLOCK_COIN:
                                        case OBJ_PSWITCH_BRICK: {
                                            bump_block(bumped, oid, true);
                                            break;
                                        }
                                    }
                                    hit = 1L;
                                }
                            } else if (is_solid(bid, true, false)) {
                                if (object->values[VAL_MISSILE_HIT] > 0L) {
                                    ++(object->values[VAL_MISSILE_HIT]);
                                } else {
                                    struct GameObject* bumped = &(state.objects[bid]);
                                    if (Fsub(object->pos[1], object->values[VAL_Y_SPEED]) <=
                                        Fadd(bumped->pos[1], bumped->bbox[0][1]))
                                        hit = 1L;
                                }
                            }
                        }

                        if (hit > 0L) {
                            create_object(OBJ_EXPLODE, object->pos);
                            object->values[VAL_X_SPEED] = -(object->values[VAL_X_SPEED]);
                            object->values[VAL_Y_SPEED] = FfInt(-8L);

                            --(object->values[VAL_MISSILE_HITS]);
                            object->values[VAL_MISSILE_HIT] = 2L;
                            if (hit == 1L)
                                play_sound_at_object(object, "HURT");
                        }
                    }

                    if (Fadd(object->pos[1], Fmin(object->values[VAL_Y_SPEED], FxZero)) > state.water) {
                        const ObjectID sid = create_object(OBJ_MISSILE_BEETROOT_SINK, object->pos);
                        if (object_is_alive(sid)) {
                            struct GameObject* pawn =
                                get_object((ObjectID)(state.objects[sid].values[VAL_MISSILE_OWNER] =
                                                          object->values[VAL_MISSILE_OWNER]));
                            if (pawn != NULL)
                                for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++)
                                    if (!object_is_alive((ObjectID)(pawn->values[i]))) {
                                        pawn->values[i] = sid;
                                        break;
                                    }
                        }

                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    const PlayerID player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
                    if ((player >= 0L && !in_player_view(object, player, FxZero, true)) ||
                        (player < 0L && !in_any_view(object, FxZero, true)))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MISSILE_BEETROOT_SINK: {
                    move_object(oid, (fvec2){object->pos[0], Fadd(object->pos[1], FfInt(random() % 3))});

                    if (object->values[VAL_MISSILE_BUBBLE] < 20L && (state.time % 10L) == 0) {
                        create_object(OBJ_BUBBLE, (fvec2){object->pos[0], Fsub(object->pos[1], FfInt(3L))});
                        ++(object->values[VAL_MISSILE_BUBBLE]);
                    }

                    const PlayerID player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
                    if ((player >= 0L && !in_player_view(object, player, FxZero, false)) ||
                        (player < 0L && !in_any_view(object, FxZero, false)))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_EXPLODE: {
                    object->values[VAL_EXPLODE_FRAME] += 24L;
                    if (object->values[VAL_EXPLODE_FRAME] >= 300L)
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_ITEM_BLOCK:
                case OBJ_BRICK_BLOCK:
                case OBJ_PSWITCH_BRICK: {
                    if (object->values[VAL_BLOCK_BUMP] > 0L)
                        if (++(object->values[VAL_BLOCK_BUMP]) > 10L)
                            object->values[VAL_BLOCK_BUMP] = 0L;
                    break;
                }

                case OBJ_BRICK_BLOCK_COIN: {
                    if (object->values[VAL_BLOCK_TIME] > 0L && object->values[VAL_BLOCK_TIME] <= 300L)
                        ++(object->values[VAL_BLOCK_TIME]);

                    if (object->values[VAL_BLOCK_BUMP] > 0L)
                        if (++(object->values[VAL_BLOCK_BUMP]) > 10L)
                            object->values[VAL_BLOCK_BUMP] = 0L;
                    break;
                }

                case OBJ_BLOCK_BUMP: {
                    struct BlockList list = {0};
                    list_block_at(
                        &list,
                        (fvec2[2]){{Fadd(object->pos[0], object->bbox[0][0]), Fadd(object->pos[1], object->bbox[0][1])},
                                   {Fadd(object->pos[0], object->bbox[1][0]), Fadd(object->pos[1], object->bbox[1][1])}}
                    );

                    for (size_t i = 0; i < list.num_objects; i++) {
                        const ObjectID bid = list.objects[i];
                        if (bid == oid || bid == object->values[VAL_BLOCK_BUMP_OWNER])
                            continue;

                        struct GameObject* bumped = &(state.objects[bid]);
                        switch (bumped->type) {
                            default:
                                break;

                            case OBJ_PLAYER: {
                                if (get_owner_id((ObjectID)(object->values[VAL_BLOCK_BUMP_OWNER])) == get_owner_id(bid))
                                    break;
                                if (bumped->values[VAL_PLAYER_GROUND] > 0L) {
                                    bumped->values[VAL_Y_SPEED] = FfInt(-8L);
                                    bumped->values[VAL_PLAYER_GROUND] = 0L;
                                }
                                break;
                            }

                            case OBJ_COIN: {
                                struct GameObject* pop = get_object(create_object(
                                    OBJ_COIN_POP,
                                    (fvec2){Fadd(bumped->pos[0], FfInt(16L)), Fadd(bumped->pos[1], FfInt(28L))}
                                ));
                                if (pop != NULL)
                                    pop->values[VAL_COIN_POP_OWNER] = object->values[VAL_BLOCK_BUMP_OWNER];
                                bumped->flags |= FLG_DESTROY;
                                break;
                            }
                        }
                    }

                    if (++(object->values[VAL_BLOCK_BUMP]) > 4L)
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_BRICK_SHARD: {
                    object->values[VAL_BRICK_SHARD_ANGLE] = Fadd(object->values[VAL_BRICK_SHARD_ANGLE], 0x00007098);

                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                    move_object(oid, POS_SPEED(object));

                    if (!in_any_view(object, FxZero, true))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MUSHROOM:
                case OBJ_MUSHROOM_1UP: {
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00004A3D);
                    displace_object(oid, 10, false);
                    if (object->values[VAL_X_TOUCH] != 0L)
                        object->values[VAL_X_SPEED] = object->values[VAL_X_TOUCH] * FfInt(-2L);

                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MUSHROOM_POISON: {
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], FxHalf);
                    displace_object(oid, 10, false);
                    if (object->values[VAL_X_TOUCH] != 0L)
                        object->values[VAL_X_SPEED] = object->values[VAL_X_TOUCH] * FfInt(-2L);

                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_STARMAN: {
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00003333);
                    displace_object(oid, 10, false);
                    if (object->values[VAL_X_TOUCH] != 0L)
                        object->values[VAL_X_SPEED] = object->values[VAL_X_TOUCH] * -0x00028000;
                    if (object->values[VAL_Y_TOUCH] > 0L)
                        object->values[VAL_Y_SPEED] = FfInt(-5);

                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_COIN_POP: {
                    if (!(object->flags & FLG_COIN_POP_START)) {
                        const PlayerID pid = get_owner_id((ObjectID)(object->values[VAL_COIN_POP_OWNER]));
                        struct GamePlayer* player = get_player(pid);
                        if (player != NULL) {
                            if (++(player->coins) >= 100L) {
                                give_points(object, pid, -1L);
                                player->coins -= 100L;
                            }
                            player->score += 200L;
                        }

                        play_sound_at_object(object, "COIN");
                        object->values[VAL_Y_SPEED] = -0x00044000;
                        object->flags |= FLG_COIN_POP_START;
                    }

                    if (object->values[VAL_Y_SPEED] < FxZero) {
                        move_object(oid, POS_SPEED(object));
                        object->values[VAL_Y_SPEED] = Fmin(Fadd(object->values[VAL_Y_SPEED], 0x00002AC1), FxZero);
                    }

                    if (object->flags & FLG_COIN_POP_SPARK) {
                        object->values[VAL_COIN_POP_FRAME] += 35L;
                        if (object->values[VAL_COIN_POP_FRAME] >= 400L) {
                            struct GameObject* points = get_object(create_object(OBJ_POINTS, object->pos));
                            if (points != NULL) {
                                points->values[VAL_POINTS_PLAYER] =
                                    (fix16_t)(get_owner_id((ObjectID)(object->values[VAL_COIN_POP_OWNER])));
                                points->values[VAL_POINTS] = 200L;
                            }
                            object->flags |= FLG_DESTROY;
                        }
                    } else {
                        object->values[VAL_COIN_POP_FRAME] += 70L;
                        if (object->values[VAL_COIN_POP_FRAME] >= 1400L) {
                            object->values[VAL_COIN_POP_FRAME] = 0L;
                            object->flags |= FLG_COIN_POP_SPARK;
                        }
                    }

                    break;
                }

                case OBJ_ROTODISC_BALL: {
                    if (object->flags & FLG_ROTODISC_START)
                        break;

                    const ObjectID rid = create_object(OBJ_ROTODISC, object->pos);
                    struct GameObject* rotodisc = get_object(rid);
                    if (rotodisc != NULL) {
                        rotodisc->values[VAL_ROTODISC_OWNER] = oid;
                        rotodisc->values[VAL_ROTODISC_ANGLE] = object->values[VAL_ROTODISC_ANGLE];
                        rotodisc->values[VAL_ROTODISC_SPEED] = object->values[VAL_ROTODISC_SPEED];
                        if (object->flags & FLG_ROTODISC_FLOWER)
                            rotodisc->flags |= FLG_ROTODISC_FLOWER;
                        else
                            rotodisc->values[VAL_ROTODISC_LENGTH] = object->values[VAL_ROTODISC_LENGTH];
                    }
                    object->values[VAL_ROTODISC] = rid;

                    object->flags |= FLG_ROTODISC_START;
                    break;
                }

                case OBJ_ROTODISC: {
                    struct GameObject* owner = get_object((ObjectID)(object->values[VAL_ROTODISC_OWNER]));
                    if (owner == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    if (object->flags & FLG_ROTODISC_FLOWER) {
                        object->values[VAL_ROTODISC_LENGTH] = Fadd(
                            object->values[VAL_ROTODISC_LENGTH],
                            FfInt((object->flags & FLG_ROTODISC_FLOWER2) ? -5L : 5L)
                        );

                        if (object->values[VAL_ROTODISC_LENGTH] >= FfInt(200L) &&
                            !(object->flags & FLG_ROTODISC_FLOWER2))
                            object->flags |= FLG_ROTODISC_FLOWER2;
                        else if (object->values[VAL_ROTODISC_LENGTH] <= FxZero &&
                                 (object->flags & FLG_ROTODISC_FLOWER2))
                            object->flags &= ~FLG_ROTODISC_FLOWER2;
                    }

                    object->values[VAL_ROTODISC_ANGLE] =
                        Fadd(object->values[VAL_ROTODISC_ANGLE], object->values[VAL_ROTODISC_SPEED]);

                    move_object(
                        oid,
                        (fvec2){Fadd(
                                    owner->pos[0],
                                    Fmul(Fcos(object->values[VAL_ROTODISC_ANGLE]), object->values[VAL_ROTODISC_LENGTH])
                                ),
                                Fadd(
                                    owner->pos[1],
                                    Fmul(Fsin(object->values[VAL_ROTODISC_ANGLE]), object->values[VAL_ROTODISC_LENGTH])
                                )}
                    );
                    break;
                }

                case OBJ_WATER_SPLASH: {
                    object->values[VAL_WATER_SPLASH_FRAME] += 7L;
                    if (object->values[VAL_WATER_SPLASH_FRAME] >= 150)
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_BUBBLE: {
                    ++(object->values[VAL_BUBBLE_FRAME]);

                    if (object->flags & FLG_BUBBLE_POP) {
                        if (object->values[VAL_BUBBLE_FRAME] >= 7L)
                            object->flags |= FLG_DESTROY;
                        break;
                    }

                    int32_t xoffs = random() % 2L;
                    xoffs -= random() % 2L;
                    move_object(
                        oid, (fvec2){Fadd(object->pos[0], FfInt(xoffs)), Fsub(object->pos[1], FfInt(random() % 3L))}
                    );
                    if (object->pos[1] < state.water) {
                        object->values[VAL_BUBBLE_FRAME] = 0L;
                        object->flags |= FLG_BUBBLE_POP;
                    }

                    if (!in_any_view(object, FfInt(28L), false))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_GOAL_BAR: {
                    if (state.sequence.type == SEQ_WIN)
                        break;

                    if (!(object->flags & FLG_GOAL_START)) {
                        object->values[VAL_GOAL_Y] = object->pos[1];
                        object->values[VAL_Y_SPEED] = FfInt(3L);
                        object->flags |= FLG_GOAL_START;
                    }

                    if ((object->values[VAL_Y_SPEED] > FxZero &&
                         object->pos[1] >= Fadd(object->values[VAL_GOAL_Y], FfInt(220L))) ||
                        (object->values[VAL_Y_SPEED] < FxZero && object->pos[1] <= object->values[VAL_GOAL_Y]))
                        object->values[VAL_Y_SPEED] = -(object->values[VAL_Y_SPEED]);
                    move_object(oid, POS_SPEED(object));
                    break;
                }

                case OBJ_GOAL_BAR_FLY: {
                    object->values[VAL_GOAL_ANGLE] = Fadd(object->values[VAL_GOAL_ANGLE], 0x00006488);

                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00003333);
                    move_object(oid, POS_SPEED(object));

                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_PSWITCH: {
                    if (object->values[VAL_PSWITCH] > 0L && !(object->flags & FLG_PSWITCH_ONCE))
                        --(object->values[VAL_PSWITCH]);
                    break;
                }

                case OBJ_AUTOSCROLL: {
                    if (state.autoscroll != oid && in_any_view(object, FxZero, false)) {
                        struct GameObject* autoscroll = get_object(state.autoscroll);
                        if (autoscroll != NULL) {
                            autoscroll->values[VAL_X_SPEED] = object->values[VAL_X_SPEED];
                            autoscroll->values[VAL_Y_SPEED] = object->values[VAL_Y_SPEED];
                            autoscroll->flags |= (object->flags & FLG_SCROLL_TANKS);
                            object->flags |= FLG_DESTROY;
                            break;
                        } else {
                            state.autoscroll = oid;
                        }
                    }
                    if (state.autoscroll == oid) {
                        move_object(oid, POS_SPEED(object));
                        if (object->flags & FLG_SCROLL_TANKS) {
                            const fix16_t end = Fsub(state.size[0], F_SCREEN_WIDTH);
                            if (object->pos[0] > end) {
                                object->values[VAL_X_SPEED] = FxZero;
                                move_object(oid, (fvec2){end, object->pos[1]});
                            }
                        }
                    }
                    break;
                }

                case OBJ_WHEEL_LEFT:
                case OBJ_WHEEL:
                case OBJ_WHEEL_RIGHT: {
                    const struct GameObject* autoscroll = get_object(state.autoscroll);
                    if (autoscroll != NULL && (autoscroll->flags & FLG_SCROLL_TANKS) &&
                        autoscroll->values[VAL_X_SPEED] != FxZero)
                        object->values[VAL_WHEEL_FRAME] = Fadd(
                            object->values[VAL_WHEEL_FRAME],
                            (autoscroll->values[VAL_X_SPEED] > FxZero) ? FxHalf : -FxHalf
                        );
                    break;
                }

                case OBJ_THWOMP: {
                    if (!(object->flags & FLG_THWOMP_START)) {
                        object->values[VAL_THWOMP_Y] = object->pos[1];
                        object->flags |= FLG_THWOMP_START;
                    }

                    if (object->flags & FLG_THWOMP_LAUGH) {
                        object->values[VAL_THWOMP_FRAME] = Fadd(object->values[VAL_THWOMP_FRAME], 0x000028F6);
                        if (object->values[VAL_THWOMP_FRAME] >= FfInt(15L)) {
                            object->values[VAL_THWOMP_FRAME] = FxZero;
                            object->flags &= ~FLG_THWOMP_LAUGH;
                        }
                    } else if (object->values[VAL_THWOMP_FRAME] > FxZero) {
                        object->values[VAL_THWOMP_FRAME] = Fadd(object->values[VAL_THWOMP_FRAME], FxOne);
                        if (object->values[VAL_THWOMP_FRAME] >= FfInt(7L))
                            object->values[VAL_THWOMP_FRAME] = FxZero;
                    }

                    if ((state.time % 5) == 0) {
                        const int32_t blink = random() % 20L;
                        if (blink == 5L && object->values[VAL_THWOMP_FRAME] <= FxZero)
                            object->values[VAL_THWOMP_FRAME] = Fadd(object->values[VAL_THWOMP_FRAME], FxOne);
                    }

                    switch (object->values[VAL_THWOMP_STATE]) {
                        default: {
                            ++(object->values[VAL_THWOMP_STATE]);
                            break;
                        }

                        case 0: {
                            const ObjectID nid = nearest_player(object->pos);
                            struct GameObject* nearest = get_object(nid);
                            if (nearest != NULL && object->pos[0] < Fadd(nearest->pos[0], FfInt(100L)) &&
                                object->pos[0] > Fsub(nearest->pos[0], FfInt(100L)) &&
                                in_player_view(object, get_owner_id(nid), FfInt(64L), false)) {
                                ++(object->values[VAL_THWOMP_STATE]);
                            }
                            break;
                        }

                        case 1: {
                            object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], FxOne);
                            displace_object(oid, FxZero, false);
                            if (object->values[VAL_Y_TOUCH] > 0L) {
                                create_object(OBJ_EXPLODE, POS_ADD(object, FfInt(-17L), FfInt(34L)));
                                create_object(OBJ_EXPLODE, POS_ADD(object, FfInt(17L), FfInt(34L)));
                                ++(object->values[VAL_THWOMP_STATE]);
                                quake_at((float[2]){FtFloat(object->pos[0]), FtFloat(object->pos[1])}, 10);
                                play_sound_at_object(object, "HURT");
                            }
                            break;
                        }

                        case 101: {
                            move_object(oid, POS_ADD(object, FxZero, -FxOne));
                            if (object->pos[1] < object->values[VAL_THWOMP_Y]) {
                                move_object(oid, (fvec2){object->pos[0], object->values[VAL_THWOMP_Y]});
                                object->values[VAL_THWOMP_STATE] = 0L;
                            }
                            break;
                        }
                    }

                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }
            }

        const ObjectID next = object->previous;
        if (object->flags & FLG_DESTROY)
            destroy_object(oid);
        oid = next;
    }

    if (state.pswitch > 0L) {
        --(state.pswitch);

        if (state.pswitch == 100L)
            play_sound("STARMAN");

        if (state.pswitch <= 0L) {
            ObjectID oid = state.live_objects;
            while (object_is_alive(oid)) {
                struct GameObject* object = &(state.objects[oid]);
                const ObjectID next = object->previous;

                if (object->type == OBJ_PSWITCH_BRICK) {
                    create_object(OBJ_COIN, object->pos);
                    destroy_object(oid);
                } else if (object->type == OBJ_PSWITCH_COIN) {
                    create_object(OBJ_BRICK_BLOCK, object->pos);
                    destroy_object(oid);
                }

                oid = next;
            }

            stop_track(TS_SWITCH);
        }
    }

    ++(state.time);
    if (state.sequence.type == SEQ_NONE && state.clock > 0L && (state.time % 25L) == 0L) {
        --(state.clock);

        if (state.clock <= 100L && !(state.flags & GF_HURRY)) {
            play_sound("HURRY");
            state.flags |= GF_HURRY;
        }

        if (state.clock <= 0L)
            for (size_t i = 0; i < MAX_PLAYERS; i++) {
                if (!(state.players[i].active))
                    continue;

                struct GameObject* pawn = get_object(state.players[i].object);
                if (pawn != NULL)
                    kill_player(pawn);
            }
    }

    if (state.sequence.time > 0L && state.sequence.time <= 300L) {
        ++(state.sequence.time);

        if (state.sequence.type == SEQ_WIN && state.sequence.time >= 50L && state.clock > 0L) {
            const int32_t diff = (state.clock > 0L) + (((state.clock - 1L) >= 10L) * 10L);
            state.clock -= diff;

            struct GamePlayer* player = get_player(state.sequence.activator);
            if (player != NULL)
                player->score += diff * 10L;

            if ((state.time % 5L) == 0L)
                play_sound("TICK");
        }

        if (state.sequence.time > 300L) {
            state.flags |= GF_END;
            INFO("Game over");
        }
    }
}

void draw_state() {
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        const struct GameObject* object = &(state.objects[oid]);
        if (object->flags & FLG_VISIBLE)
            switch (object->type) {
                default:
                    break;

                case OBJ_CLOUD: {
                    const char* tex;
                    switch (((int)((float)state.time / 12.5f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = "S_CLOUDA";
                            break;
                        case 1:
                        case 3:
                            tex = "S_CLOUDB";
                            break;
                        case 2:
                            tex = "S_CLOUDC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUSH: {
                    const char* tex;
                    switch (((int)((float)state.time / 7.142857142857143f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = "S_BUSHA";
                            break;
                        case 1:
                        case 3:
                            tex = "S_BUSHB";
                            break;
                        case 2:
                            tex = "S_BUSHC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUSH_SNOW: {
                    const char* tex;
                    switch (((int)((float)state.time / 7.142857142857143f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = "S_BUSHSA";
                            break;
                        case 1:
                        case 3:
                            tex = "S_BUSHSB";
                            break;
                        case 2:
                            tex = "S_BUSHSC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_PLAYER: {
                    if (object->values[VAL_PLAYER_FLASH] > 0L && (state.time % 2))
                        break;

                    const struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_PLAYER_INDEX]));
                    const char* tex =
                        get_player_texture(player == NULL ? POW_SMALL : player->power, get_player_frame(object));
                    const GLubyte a = (object->values[VAL_PLAYER_INDEX] != local_player) ? 190 : 255;
                    draw_object(oid, tex, 0, ALPHA(a));

                    if (object->values[VAL_PLAYER_STARMAN] > 0L) {
                        GLubyte r, g, b;
                        switch (state.time % 5) {
                            default: {
                                r = 248;
                                g = 0;
                                b = 0;
                                break;
                            }
                            case 1: {
                                r = 192;
                                g = 152;
                                b = 0;
                                break;
                            }
                            case 2: {
                                r = 144;
                                g = 192;
                                b = 40;
                                break;
                            }
                            case 3: {
                                r = 88;
                                g = 136;
                                b = 232;
                                break;
                            }
                            case 4: {
                                r = 192;
                                g = 120;
                                b = 200;
                                break;
                            }
                        }

                        set_batch_stencil(1);
                        set_batch_logic(GL_XOR);
                        draw_object(oid, tex, 0, RGBA(r, g, b, a));
                        set_batch_logic(GL_COPY);
                        set_batch_stencil(0);
                    }
                    break;
                }

                case OBJ_PLAYER_DEAD: {
                    draw_object(oid, "P_DEAD", 0, ALPHA(object->values[VAL_PLAYER_INDEX] != local_player ? 190 : 255));
                    break;
                }

                case OBJ_KEVIN: {
                    const struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_KEVIN_PLAYER]));
                    if (player == NULL)
                        break;

                    const struct KevinFrame* kframe = &(player->kevin.frames[0]);
                    const char* tex = get_player_texture(kframe->power, kframe->frame);
                    const GLubyte a = (object->values[VAL_KEVIN_PLAYER] != local_player) ? 160 : 210;

                    const struct InterpObject* smooth = &(interp[oid]);
                    float pos[3] = {FtInt(smooth->pos[0]), FtInt(smooth->pos[1]), FtFloat(object->depth)};
                    bool flip[2] = {object->flags & FLG_X_FLIP, false};

                    draw_sprite(tex, pos, flip, 0, RGBA(80, 80, 80, a));
                    pos[0] += (float)(object->values[VAL_KEVIN_X_JITTER]);
                    pos[1] += (float)(object->values[VAL_KEVIN_Y_JITTER]);
                    draw_sprite(tex, pos, flip, 0, RGBA(80, 80, 80, a * 0.75f));
                    break;
                }

                case OBJ_PLAYER_EFFECT: {
                    draw_object(
                        oid,
                        get_player_texture(
                            object->values[VAL_PLAYER_EFFECT_POWER], object->values[VAL_PLAYER_EFFECT_FRAME]
                        ),
                        0, ALPHA(FtInt(object->values[VAL_PLAYER_EFFECT_ALPHA]))
                    );
                    break;
                }

                case OBJ_COIN:
                case OBJ_PSWITCH_COIN: {
                    const char* tex;
                    switch ((state.time / 5) % 4) {
                        default:
                            tex = "I_COINA";
                            break;
                        case 1:
                        case 3:
                            tex = "I_COINB";
                            break;
                        case 2:
                            tex = "I_COINC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_COIN_POP: {
                    const char* tex;
                    if (object->flags & FLG_COIN_POP_SPARK)
                        switch (object->values[VAL_COIN_POP_FRAME] / 100) {
                            default:
                                tex = "I_CPOPE";
                                break;
                            case 1:
                                tex = "I_CPOPF";
                                break;
                            case 2:
                                tex = "I_CPOPG";
                                break;
                            case 3:
                                tex = "I_CPOPH";
                                break;
                        }
                    else
                        switch ((object->values[VAL_COIN_POP_FRAME] / 100) % 5) {
                            default:
                                tex = "I_CPOPA";
                                break;
                            case 2:
                                tex = "I_CPOPB";
                                break;
                            case 3:
                                tex = "I_CPOPC";
                                break;
                            case 4:
                                tex = "I_CPOPD";
                                break;
                        }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM: {
                    draw_object(oid, "I_SHROOM", 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM_1UP: {
                    draw_object(oid, "I_1UP", 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM_POISON: {
                    draw_object(
                        oid, ((int)((float)(state.time) / 11.11111111111111f) % 2) ? "I_POISOB" : "I_POISOA", 0, WHITE
                    );
                    break;
                }

                case OBJ_FIRE_FLOWER: {
                    const char* tex;
                    switch ((int)((float)state.time / 3.703703703703704f) % 4) {
                        default:
                            tex = "I_FLWRA";
                            break;
                        case 1:
                            tex = "I_FLWRB";
                            break;
                        case 2:
                            tex = "I_FLWRC";
                            break;
                        case 3:
                            tex = "I_FLWRD";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BEETROOT: {
                    const char* tex;
                    switch ((int)((float)state.time / 12.5f) % 4) {
                        default:
                            tex = "I_BEETA";
                            break;
                        case 1:
                        case 3:
                            tex = "I_BEETB";
                            break;
                        case 2:
                            tex = "I_BEETC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_LUI: {
                    const char* tex;
                    if (object->values[VAL_LUI_BOUNCE] > 0L)
                        switch (object->values[VAL_LUI_BOUNCE] / 100) {
                            default:
                            case 0:
                                tex = "I_LUIB";
                                break;
                            case 1:
                            case 5:
                                tex = "I_LUIF";
                                break;
                            case 2:
                            case 4:
                                tex = "I_LUIG";
                                break;
                            case 3:
                                tex = "I_LUIH";
                                break;
                        }
                    else
                        switch (state.time % 12) {
                            default:
                            case 10:
                            case 11:
                                tex = "I_LUIA";
                                break;
                            case 0:
                                tex = "I_LUIB";
                                break;
                            case 1:
                            case 8:
                            case 9:
                                tex = "I_LUIC";
                                break;
                            case 2:
                            case 6:
                            case 7:
                                tex = "I_LUID";
                                break;
                            case 3:
                            case 4:
                            case 5:
                                tex = "I_LUIE";
                                break;
                        }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    draw_object(oid, "I_HAMMER", 0, WHITE);
                    break;
                }

                case OBJ_STARMAN: {
                    const char* tex;
                    switch ((int)((float)state.time / 2.040816326530612f) % 4) {
                        default:
                            tex = "I_STARA";
                            break;
                        case 1:
                            tex = "I_STARB";
                            break;
                        case 2:
                            tex = "I_STARC";
                            break;
                        case 3:
                            tex = "I_STARD";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_POINTS: {
                    if (object->values[VAL_POINTS_PLAYER] != local_player)
                        break;

                    const char* tex;
                    switch (object->values[VAL_POINTS]) {
                        default:
                        case 100:
                            tex = "H_100";
                            break;
                        case 200:
                            tex = "H_200";
                            break;
                        case 500:
                            tex = "H_500";
                            break;
                        case 1000:
                            tex = "H_1000";
                            break;
                        case 2000:
                            tex = "H_2000";
                            break;
                        case 5000:
                            tex = "H_5000";
                            break;
                        case 10000:
                            tex = "H_10000";
                            break;
                        case 1000000:
                            tex = "H_1M";
                            break;
                        case -1:
                            tex = "H_1UP";
                            break;
                    }

                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    draw_object(oid, "R_FIRE", FtFloat(object->values[VAL_MISSILE_ANGLE]), WHITE);
                    break;
                }

                case OBJ_MISSILE_BEETROOT:
                case OBJ_MISSILE_BEETROOT_SINK: {
                    draw_object(oid, "R_BEET", 0, WHITE);
                    break;
                }

                case OBJ_MISSILE_HAMMER: {
                    draw_object(oid, "R_HAMMRA", FtFloat(object->values[VAL_MISSILE_ANGLE]), WHITE);
                    break;
                }

                case OBJ_EXPLODE: {
                    const char* tex;
                    switch (object->values[VAL_EXPLODE_FRAME] / 100) {
                        default:
                            tex = "X_BOOMA";
                            break;
                        case 1:
                            tex = "X_BOOMB";
                            break;
                        case 2:
                            tex = "X_BOOMC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_ITEM_BLOCK: {
                    const char* tex;
                    if (object->flags & FLG_BLOCK_EMPTY)
                        tex = "I_EMPTY";
                    else
                        switch ((int)((float)(state.time) / 11.11111111111111f) % 4) {
                            default:
                                tex = "I_BLOCKA";
                                break;
                            case 1:
                            case 3:
                                tex = "I_BLOCKB";
                                break;
                            case 2:
                                tex = "I_BLOCKC";
                                break;
                        }

                    int8_t bump;
                    switch (object->values[VAL_BLOCK_BUMP]) {
                        default:
                            bump = 0;
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

                    draw_sprite(
                        tex,
                        (float[3]){
                            FtInt(object->pos[0]),
                            FtInt(object->pos[1]) - bump,
                            FtFloat(object->depth),
                        },
                        (bool[2]){false}, 0, WHITE
                    );
                    break;
                }

                case OBJ_BRICK_BLOCK:
                case OBJ_PSWITCH_BRICK: {
                    int8_t bump;
                    switch (object->values[VAL_BLOCK_BUMP]) {
                        default:
                            bump = 0;
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

                    draw_sprite(
                        (object->flags & FLG_BLOCK_GRAY) ? "I_BRICKB" : "I_BRICKA",
                        (float[3]){
                            FtInt(object->pos[0]),
                            FtInt(object->pos[1]) - bump,
                            FtFloat(object->depth),
                        },
                        (bool[2]){false}, 0, WHITE
                    );
                    break;
                }

                case OBJ_BRICK_BLOCK_COIN: {
                    int8_t bump;
                    if ((object->flags & FLG_BLOCK_EMPTY) || object->values[VAL_BLOCK_ITEM] != OBJ_COIN_POP)
                        switch (object->values[VAL_BLOCK_BUMP]) {
                            default:
                                bump = 0;
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
                    else
                        switch (object->values[VAL_BLOCK_BUMP]) {
                            default:
                                bump = 0;
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

                    draw_sprite(
                        (object->flags & FLG_BLOCK_EMPTY)
                            ? "I_EMPTY"
                            : ((object->flags & FLG_BLOCK_GRAY) ? "I_BRICKB" : "I_BRICKA"),
                        (float[3]){
                            FtInt(object->pos[0]),
                            FtInt(object->pos[1]) - bump,
                            FtFloat(object->depth),
                        },
                        (bool[2]){false}, 0, WHITE
                    );
                    break;
                }

                case OBJ_BRICK_SHARD: {
                    draw_object(
                        oid, (object->flags & FLG_BLOCK_GRAY) ? "X_SHARDB" : "X_SHARDA",
                        FtFloat(object->values[VAL_BRICK_SHARD_ANGLE]), WHITE
                    );
                    break;
                }

                case OBJ_CHECKPOINT: {
                    draw_object(
                        oid,
                        (state.checkpoint == oid) ? (((state.time / 10) % 2) ? "M_CHECKC" : "M_CHECKB") : "M_CHECKA", 0,
                        ALPHA(oid >= state.checkpoint ? 255 : 128)
                    );
                    break;
                }

                case OBJ_ROTODISC_BALL: {
                    draw_object(oid, "E_RDBALL", 0, WHITE);
                    break;
                }

                case OBJ_ROTODISC: {
                    const char* tex;
                    switch (state.time % 26) {
                        default:
                            tex = "E_ROTODA";
                            break;
                        case 1:
                            tex = "E_ROTODB";
                            break;
                        case 2:
                            tex = "E_ROTODC";
                            break;
                        case 3:
                            tex = "E_ROTODD";
                            break;
                        case 4:
                            tex = "E_ROTODE";
                            break;
                        case 5:
                            tex = "E_ROTODF";
                            break;
                        case 6:
                            tex = "E_ROTODG";
                            break;
                        case 7:
                            tex = "E_ROTODH";
                            break;
                        case 8:
                            tex = "E_ROTODI";
                            break;
                        case 9:
                            tex = "E_ROTODJ";
                            break;
                        case 10:
                            tex = "E_ROTODK";
                            break;
                        case 11:
                            tex = "E_ROTODL";
                            break;
                        case 12:
                            tex = "E_ROTODM";
                            break;
                        case 13:
                            tex = "E_ROTODN";
                            break;
                        case 14:
                            tex = "E_ROTODO";
                            break;
                        case 15:
                            tex = "E_ROTODP";
                            break;
                        case 16:
                            tex = "E_ROTODQ";
                            break;
                        case 17:
                            tex = "E_ROTODR";
                            break;
                        case 18:
                            tex = "E_ROTODS";
                            break;
                        case 19:
                            tex = "E_ROTODT";
                            break;
                        case 20:
                            tex = "E_ROTODU";
                            break;
                        case 21:
                            tex = "E_ROTODV";
                            break;
                        case 22:
                            tex = "E_ROTODW";
                            break;
                        case 23:
                            tex = "E_ROTODX";
                            break;
                        case 24:
                            tex = "E_ROTODY";
                            break;
                        case 25:
                            tex = "E_ROTODZ";
                            break;
                    }

                    set_batch_logic(GL_OR_REVERSE);
                    draw_object(oid, tex, 0, WHITE);
                    set_batch_logic(GL_COPY);
                    break;
                }

                case OBJ_WATER_SPLASH: {
                    const char* tex;
                    switch (object->values[VAL_WATER_SPLASH_FRAME] / 10) {
                        default:
                            tex = "X_SPLSHA";
                            break;
                        case 1:
                            tex = "X_SPLSHB";
                            break;
                        case 2:
                            tex = "X_SPLSHC";
                            break;
                        case 3:
                            tex = "X_SPLSHD";
                            break;
                        case 4:
                            tex = "X_SPLSHE";
                            break;
                        case 5:
                            tex = "X_SPLSHF";
                            break;
                        case 6:
                            tex = "X_SPLSHG";
                            break;
                        case 7:
                            tex = "X_SPLSHH";
                            break;
                        case 8:
                            tex = "X_SPLSHI";
                            break;
                        case 9:
                            tex = "X_SPLSHJ";
                            break;
                        case 10:
                            tex = "X_SPLSHK";
                            break;
                        case 11:
                            tex = "X_SPLSHL";
                            break;
                        case 12:
                            tex = "X_SPLSHM";
                            break;
                        case 13:
                            tex = "X_SPLSHN";
                            break;
                        case 14:
                            tex = "X_SPLSHO";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUBBLE: {
                    const char* tex;
                    float pos[3] = {FtInt(object->pos[0]), FtInt(object->pos[1]), FtFloat(object->depth)};
                    if (object->flags & FLG_BUBBLE_POP) {
                        switch (object->values[VAL_BUBBLE_FRAME]) {
                            default:
                                tex = "X_BUBBLB";
                                break;
                            case 1:
                                tex = "X_BUBBLC";
                                break;
                            case 2:
                                tex = "X_BUBBLD";
                                break;
                            case 3:
                                tex = "X_BUBBLE";
                                break;
                            case 4:
                                tex = "X_BUBBLF";
                                break;
                            case 5:
                                tex = "X_BUBBLG";
                                break;
                            case 6:
                                tex = "X_BUBBLH";
                                break;
                        }
                    } else {
                        tex = "X_BUBBLA";
                        switch ((object->values[VAL_BUBBLE_FRAME] / 2) % 5) {
                            default:
                                break;
                            case 1:
                                pos[0] -= 1;
                                break;
                            case 2:
                                pos[0] += 1;
                                break;
                            case 4:
                                pos[1] -= 2;
                                break;
                        }
                    }

                    draw_sprite(tex, pos, (bool[2]){false}, 0, WHITE);
                    break;
                }

                case OBJ_GOAL_BAR: {
                    draw_object(oid, "M_GOALBA", 0, WHITE);
                    break;
                }

                case OBJ_GOAL_BAR_FLY: {
                    draw_object(oid, "M_GOALBB", FtFloat(object->values[VAL_GOAL_ANGLE]), WHITE);
                    break;
                }

                case OBJ_PSWITCH: {
                    const char* tex;
                    if (object->values[VAL_PSWITCH] > 0L)
                        tex = "M_PSWITD";
                    else
                        switch ((int)((float)state.time / 3.703703703703704f) % 3) {
                            default:
                                tex = "M_PSWITA";
                                break;
                            case 1:
                                tex = "M_PSWITB";
                                break;
                            case 2:
                                tex = "M_PSWITC";
                                break;
                        }

                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_AUTOSCROLL: {
                    if (object->flags & FLG_SCROLL_TANKS) {
                        const struct InterpObject* smooth = &(interp[oid]);
                        const float x = FtInt(smooth->pos[0]);
                        const float y = FtInt(smooth->pos[1]) + SCREEN_HEIGHT - 64;
                        draw_rectangle(
                            "M_PTANKS", (float[2][2]){{x - 32, y}, {x + SCREEN_WIDTH + 32, y + 64}}, 20, WHITE
                        );
                    }
                    break;
                }

                case OBJ_WHEEL_LEFT: {
                    const char* tex;
                    switch (FtInt(object->values[VAL_WHEEL_FRAME]) % 3) {
                        default:
                            tex = "T_WHELLA";
                            break;
                        case 1:
                            tex = "T_WHELLB";
                            break;
                        case 2:
                            tex = "T_WHELLC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_WHEEL: {
                    const char* tex;
                    switch (FtInt(object->values[VAL_WHEEL_FRAME]) % 4) {
                        default:
                            tex = "T_WHEELA";
                            break;
                        case 1:
                            tex = "T_WHEELB";
                            break;
                        case 2:
                            tex = "T_WHEELC";
                            break;
                        case 3:
                            tex = "T_WHEELD";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_WHEEL_RIGHT: {
                    const char* tex;
                    switch (FtInt(object->values[VAL_WHEEL_FRAME]) % 3) {
                        default:
                            tex = "T_WHELRA";
                            break;
                        case 1:
                            tex = "T_WHELRB";
                            break;
                        case 2:
                            tex = "T_WHELRC";
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_THWOMP: {
                    const char* tex;
                    if (object->flags & FLG_THWOMP_LAUGH)
                        switch (FtInt(object->values[VAL_THWOMP_FRAME]) % 3) {
                            default:
                                tex = "E_THWMPF";
                                break;
                            case 1:
                                tex = "E_THWMPG";
                                break;
                            case 2:
                                tex = "E_THWMPH";
                                break;
                        }
                    else
                        switch (FtInt(object->values[VAL_THWOMP_FRAME]) % 7) {
                            default:
                                tex = "E_THWMPA";
                                break;
                            case 1:
                                tex = "E_THWMPB";
                                break;
                            case 2:
                            case 6:
                                tex = "E_THWMPC";
                                break;
                            case 3:
                            case 5:
                                tex = "E_THWMPD";
                                break;
                            case 4:
                                tex = "E_THWMPE";
                                break;
                        }

                    draw_object(oid, tex, 0, WHITE);
                    break;
                }
            }

        oid = object->previous;
    }

    const char* tex;
    switch ((state.time / 5) % 8) {
        default:
            tex = "M_WATERA";
            break;
        case 1:
            tex = "M_WATERB";
            break;
        case 2:
            tex = "M_WATERC";
            break;
        case 3:
            tex = "M_WATERD";
            break;
        case 4:
            tex = "M_WATERE";
            break;
        case 5:
            tex = "M_WATERF";
            break;
        case 6:
            tex = "M_WATERG";
            break;
        case 7:
            tex = "M_WATERH";
            break;
    }
    const float x1 = 0;
    const float y1 = FtInt(state.water);
    const float x2 = FtInt(state.size[0]);
    const float y2 = FtInt(state.size[1]);
    draw_rectangle(tex, (float[2][2]){{x1, y1}, {x2, y1 + 16}}, -100, ALPHA(135));
    draw_rectangle("", (float[2][2]){{x1, y1 + 16}, {x2, y2}}, -100, RGBA(88, 136, 224, 135));
}

void draw_state_hud() {
    const struct GamePlayer* player = get_player(local_player);
    if (player != NULL) {
        static char str[16];
        SDL_snprintf(str, sizeof(str), "MARIO * %u", player->lives);
        draw_text(FNT_HUD, FA_LEFT, str, (float[3]){32, 16, 0});

        SDL_snprintf(str, sizeof(str), "%u", player->score);
        draw_text(FNT_HUD, FA_RIGHT, str, (float[3]){149, 34, 0});

        const char* tex;
        switch ((int)((float)(state.time) / 6.25f) % 4) {
            default:
                tex = "H_COINSA";
                break;
            case 1:
            case 3:
                tex = "H_COINSB";
                break;
            case 2:
                tex = "H_COINSC";
                break;
        }
        draw_sprite(tex, (float[3]){224, 34, 0}, (bool[2]){false}, 0, WHITE);
        SDL_snprintf(str, sizeof(str), "%u", player->coins);
        draw_text(FNT_HUD, FA_RIGHT, str, (float[3]){288, 34, 0});

        if (state.clock >= 0L) {
            draw_text(FNT_HUD, FA_RIGHT, "TIME", (float[3]){608, 16, 0});
            SDL_snprintf(str, sizeof(str), "%u", state.clock);
            draw_text(FNT_HUD, FA_RIGHT, str, (float[3]){608, 34, 0});
        }
    }

    if (state.sequence.type == SEQ_LOSE && state.sequence.time > 0L)
        draw_text(FNT_HUD, FA_CENTER, (state.clock == 0) ? "TIME UP" : "GAME OVER", (float[3]){320, 224, 0});
}

void load_object(GameObjectType type) {
    switch (type) {
        default:
            break;

        case OBJ_CLOUD: {
            load_texture("S_CLOUDA");
            load_texture("S_CLOUDB");
            load_texture("S_CLOUDC");
            break;
        }

        case OBJ_BUSH: {
            load_texture("S_BUSHA");
            load_texture("S_BUSHB");
            load_texture("S_BUSHC");
            break;
        }

        case OBJ_BUSH_SNOW: {
            load_texture("S_BUSHSA");
            load_texture("S_BUSHSB");
            load_texture("S_BUSHSC");
            break;
        }

        case OBJ_PLAYER_SPAWN: {
            load_object(OBJ_PLAYER);
            break;
        }

        case OBJ_PLAYER: {
            load_texture("P_S_IDLE");
            load_texture("P_S_WLKA");
            load_texture("P_S_WLKB");
            load_texture("P_S_JUMP");
            load_texture("P_S_SWMA");
            load_texture("P_S_SWMB");
            load_texture("P_S_SWMC");
            load_texture("P_S_SWMD");

            load_texture("P_GROWA");
            load_texture("P_B_IDLE");
            load_texture("P_B_WLKA");
            load_texture("P_B_WLKB");
            load_texture("P_B_JUMP");
            load_texture("P_B_DUCK");
            load_texture("P_B_SWMA");
            load_texture("P_B_SWMB");
            load_texture("P_B_SWMC");
            load_texture("P_B_SWMD");

            load_texture("P_GROWB");
            load_texture("P_GROWC");
            load_texture("P_F_IDLE");
            load_texture("P_F_WLKA");
            load_texture("P_F_WLKB");
            load_texture("P_F_JUMP");
            load_texture("P_F_DUCK");
            load_texture("P_F_FIRE");
            load_texture("P_F_SWMA");
            load_texture("P_F_SWMB");
            load_texture("P_F_SWMC");
            load_texture("P_F_SWMD");

            load_texture("P_P_IDLE");
            load_texture("P_P_WLKA");
            load_texture("P_P_WLKB");
            load_texture("P_P_JUMP");
            load_texture("P_P_DUCK");
            load_texture("P_P_FIRE");
            load_texture("P_P_SWMA");
            load_texture("P_P_SWMB");
            load_texture("P_P_SWMC");
            load_texture("P_P_SWMD");

            load_texture("P_L_IDLE");
            load_texture("P_L_WLKA");
            load_texture("P_L_WLKB");
            load_texture("P_L_JUMP");
            load_texture("P_L_DUCK");
            load_texture("P_L_SWMA");
            load_texture("P_L_SWMB");
            load_texture("P_L_SWMC");
            load_texture("P_L_SWMD");

            load_texture("P_H_IDLE");
            load_texture("P_H_WLKA");
            load_texture("P_H_WLKB");
            load_texture("P_H_JUMP");
            load_texture("P_H_DUCK");
            load_texture("P_H_FIRE");
            load_texture("P_H_SWMA");
            load_texture("P_H_SWMB");
            load_texture("P_H_SWMC");
            load_texture("P_H_SWMD");

            load_texture("H_COINSA");
            load_texture("H_COINSB");
            load_texture("H_COINSC");

            load_font(FNT_HUD);

            load_sound("JUMP");
            load_sound("FIRE");
            load_sound("GROW");
            load_sound("WARP");
            load_sound("BUMP");
            load_sound("STOMP");
            load_sound("STARMAN");
            load_sound("SWIM");
            load_sound("RESPAWN");

            load_track("STARMAN");

            load_object(OBJ_PLAYER_EFFECT);
            load_object(OBJ_PLAYER_DEAD);
            load_object(OBJ_KEVIN);
            load_object(OBJ_MISSILE_FIREBALL);
            load_object(OBJ_MISSILE_BEETROOT);
            load_object(OBJ_MISSILE_HAMMER);
            load_object(OBJ_BUBBLE);
            break;
        }

        case OBJ_PLAYER_DEAD: {
            load_texture("P_DEAD");

            load_sound("LOSE");
            load_sound("DEAD");
            load_sound("HARDCORE");

            load_track("LOSE");
            load_track("LOSE2");
            load_track("GAMEOVER");
            load_track("WIN");
            load_track("WIN2");
            break;
        }

        case OBJ_KEVIN: {
            load_sound("KEVINACT");
            load_sound("KEVINDIE");

            load_object(OBJ_EXPLODE);
            break;
        }

        case OBJ_COIN:
        case OBJ_PSWITCH_COIN: {
            load_texture("I_COINA");
            load_texture("I_COINB");
            load_texture("I_COINC");

            load_sound("COIN");

            load_object(OBJ_POINTS);
            load_object(OBJ_COIN_POP);
            break;
        }

        case OBJ_COIN_POP: {
            load_texture("I_CPOPA");
            load_texture("I_CPOPB");
            load_texture("I_CPOPC");
            load_texture("I_CPOPD");
            load_texture("I_CPOPE");
            load_texture("I_CPOPF");
            load_texture("I_CPOPG");
            load_texture("I_CPOPH");

            load_sound("COIN");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM: {
            load_texture("I_SHROOM");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM_1UP: {
            load_texture("I_1UP");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM_POISON: {
            load_texture("I_POISOA");
            load_texture("I_POISOB");

            load_object(OBJ_EXPLODE);
            break;
        }

        case OBJ_FIRE_FLOWER: {
            load_texture("I_FLWRA");
            load_texture("I_FLWRB");
            load_texture("I_FLWRC");
            load_texture("I_FLWRD");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_BEETROOT: {
            load_texture("I_BEETA");
            load_texture("I_BEETB");
            load_texture("I_BEETC");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_LUI: {
            load_texture("I_LUIA");
            load_texture("I_LUIB");
            load_texture("I_LUIC");
            load_texture("I_LUID");
            load_texture("I_LUIE");
            load_texture("I_LUIF");
            load_texture("I_LUIG");
            load_texture("I_LUIH");

            load_sound("KICK");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_HAMMER_SUIT: {
            load_texture("I_HAMMER");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_STARMAN: {
            load_texture("I_STARA");
            load_texture("I_STARB");
            load_texture("I_STARC");
            load_texture("I_STARD");
            break;
        }

        case OBJ_POINTS: {
            load_texture("H_100");
            load_texture("H_200");
            load_texture("H_500");
            load_texture("H_1000");
            load_texture("H_2000");
            load_texture("H_5000");
            load_texture("H_10000");
            load_texture("H_1M");
            load_texture("H_1UP");

            load_sound("1UP");
            break;
        }

        case OBJ_MISSILE_FIREBALL: {
            load_texture("R_FIRE");

            load_sound("BUMP");
            load_sound("KICK");

            load_object(OBJ_EXPLODE);
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MISSILE_BEETROOT: {
            load_texture("R_BEET");

            load_sound("HURT");
            load_sound("BUMP");
            load_sound("KICK");

            load_object(OBJ_EXPLODE);
            load_object(OBJ_POINTS);
            load_object(OBJ_MISSILE_BEETROOT_SINK);
            break;
        }

        case OBJ_MISSILE_BEETROOT_SINK: {
            load_texture("R_BEET");

            load_object(OBJ_BUBBLE);
            break;
        }

        case OBJ_MISSILE_HAMMER: {
            load_texture("R_HAMMRA");

            load_sound("KICK");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_EXPLODE: {
            load_texture("X_BOOMA");
            load_texture("X_BOOMB");
            load_texture("X_BOOMC");
            break;
        }

        case OBJ_ITEM_BLOCK: {
            load_texture("I_BLOCKA");
            load_texture("I_BLOCKB");
            load_texture("I_BLOCKC");
            load_texture("I_EMPTY");

            load_sound("SPROUT");

            load_object(OBJ_BLOCK_BUMP);
            break;
        }

        case OBJ_BRICK_BLOCK:
        case OBJ_PSWITCH_BRICK: {
            load_texture("I_BRICKA");
            load_texture("I_BRICKB");

            load_sound("BUMP");
            load_sound("BREAK");

            load_object(OBJ_BRICK_SHARD);
            break;
        }

        case OBJ_BRICK_SHARD: {
            load_texture("X_SHARDA");
            load_texture("X_SHARDB");
            break;
        }

        case OBJ_BRICK_BLOCK_COIN: {
            load_texture("I_BRICKA");
            load_texture("I_EMPTY");

            load_object(OBJ_COIN_POP);
            break;
        }

        case OBJ_CHECKPOINT: {
            load_texture("M_CHECKA");
            load_texture("M_CHECKB");
            load_texture("M_CHECKC");

            load_sound("SPROUT");

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_ROTODISC_BALL: {
            load_texture("E_RDBALL");
            load_object(OBJ_ROTODISC);
            break;
        }

        case OBJ_ROTODISC: {
            load_texture("E_ROTODA");
            load_texture("E_ROTODB");
            load_texture("E_ROTODC");
            load_texture("E_ROTODD");
            load_texture("E_ROTODE");
            load_texture("E_ROTODF");
            load_texture("E_ROTODG");
            load_texture("E_ROTODH");
            load_texture("E_ROTODI");
            load_texture("E_ROTODJ");
            load_texture("E_ROTODK");
            load_texture("E_ROTODL");
            load_texture("E_ROTODM");
            load_texture("E_ROTODN");
            load_texture("E_ROTODO");
            load_texture("E_ROTODP");
            load_texture("E_ROTODQ");
            load_texture("E_ROTODR");
            load_texture("E_ROTODS");
            load_texture("E_ROTODT");
            load_texture("E_ROTODU");
            load_texture("E_ROTODV");
            load_texture("E_ROTODW");
            load_texture("E_ROTODX");
            load_texture("E_ROTODY");
            load_texture("E_ROTODZ");
            break;
        }

        case OBJ_GOAL_MARK: {
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_WATER_SPLASH: {
            load_texture("X_SPLSHA");
            load_texture("X_SPLSHB");
            load_texture("X_SPLSHC");
            load_texture("X_SPLSHD");
            load_texture("X_SPLSHE");
            load_texture("X_SPLSHF");
            load_texture("X_SPLSHG");
            load_texture("X_SPLSHH");
            load_texture("X_SPLSHI");
            load_texture("X_SPLSHJ");
            load_texture("X_SPLSHK");
            load_texture("X_SPLSHL");
            load_texture("X_SPLSHM");
            load_texture("X_SPLSHN");
            load_texture("X_SPLSHO");
            break;
        }

        case OBJ_BUBBLE: {
            load_texture("X_BUBBLA");
            load_texture("X_BUBBLB");
            load_texture("X_BUBBLC");
            load_texture("X_BUBBLD");
            load_texture("X_BUBBLE");
            load_texture("X_BUBBLF");
            load_texture("X_BUBBLG");
            load_texture("X_BUBBLH");
            break;
        }

        case OBJ_GOAL_BAR: {
            load_texture("M_GOALBA");

            load_object(OBJ_GOAL_BAR_FLY);
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_GOAL_BAR_FLY: {
            load_texture("M_GOALBB");
            break;
        }

        case OBJ_PSWITCH: {
            load_texture("M_PSWITA");
            load_texture("M_PSWITB");
            load_texture("M_PSWITC");
            load_texture("M_PSWITD");

            load_sound("TOGGLE");
            load_sound("STARMAN");

            load_track("PSWITCH");

            load_object(OBJ_PSWITCH_COIN);
            load_object(OBJ_PSWITCH_BRICK);
            break;
        }

        case OBJ_AUTOSCROLL: {
            load_texture("M_PTANKS");
            break;
        }

        case OBJ_WHEEL_LEFT: {
            load_texture("T_WHELLA");
            load_texture("T_WHELLB");
            load_texture("T_WHELLC");
            break;
        }

        case OBJ_WHEEL: {
            load_texture("T_WHEELA");
            load_texture("T_WHEELB");
            load_texture("T_WHEELC");
            load_texture("T_WHEELD");
            break;
        }

        case OBJ_WHEEL_RIGHT: {
            load_texture("T_WHELRA");
            load_texture("T_WHELRB");
            load_texture("T_WHELRC");
            break;
        }

        case OBJ_THWOMP: {
            load_texture("E_THWMPA");
            load_texture("E_THWMPB");
            load_texture("E_THWMPC");
            load_texture("E_THWMPD");
            load_texture("E_THWMPE");
            load_texture("E_THWMPF");
            load_texture("E_THWMPG");
            load_texture("E_THWMPH");

            load_sound("HURT");
            load_sound("THWOMP");

            load_object(OBJ_EXPLODE);
            break;
        }
    }
}

Bool object_is_alive(ObjectID index) {
    return index >= 0L && index < MAX_OBJECTS && state.objects[index].type != OBJ_NULL;
}

struct GameObject* get_object(ObjectID index) {
    return object_is_alive(index) ? &(state.objects[index]) : NULL;
}

ObjectID create_object(GameObjectType type, const fvec2 pos) {
    if (type <= OBJ_NULL || type >= OBJ_SIZE) {
        INFO("Invalid object type %u", type);
        return NULLOBJ;
    }

    ObjectID index = state.next_object;
    for (size_t i = 0; i < MAX_OBJECTS; i++) {
        if (!object_is_alive((ObjectID)index)) {
            load_object(type);
            struct GameObject* object = &state.objects[index];
            SDL_memset(object, 0, sizeof(struct GameObject));

            object->type = type;
            object->flags = FLG_VISIBLE;
            object->previous = state.live_objects;
            object->next = NULLOBJ;
            if (object_is_alive(state.live_objects))
                state.objects[state.live_objects].next = index;
            state.live_objects = index;

            object->block = -1L;
            object->previous_block = object->next_block = NULLOBJ;
            move_object(index, pos);

            interp[index].type = type;
            interp[index].from[0] = interp[index].to[0] = interp[index].pos[0] = pos[0];
            interp[index].from[1] = interp[index].to[1] = interp[index].pos[1] = pos[1];

            switch (type) {
                default:
                    break;

                case OBJ_SOLID:
                case OBJ_SOLID_TOP:
                case OBJ_SOLID_SLOPE:
                case OBJ_AUTOSCROLL:
                case OBJ_WHEEL_LEFT:
                case OBJ_WHEEL:
                case OBJ_WHEEL_RIGHT: {
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(32L);
                    break;
                }

                case OBJ_CLOUD:
                case OBJ_BUSH:
                case OBJ_BUSH_SNOW: {
                    object->depth = FfInt(25L);

                    object->values[VAL_PROP_FRAME] = random() % 4;
                    break;
                }

                case OBJ_PLAYER_SPAWN: {
                    state.spawn = (ObjectID)index;
                    break;
                }

                case OBJ_PLAYER: {
                    object->depth = -FxOne;

                    object->bbox[0][0] = FfInt(-9L);
                    object->bbox[0][1] = FfInt(-25L);
                    object->bbox[1][0] = FfInt(10L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_PLAYER_INDEX] = -1L;
                    for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                        object->values[i] = NULLOBJ;
                    for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++)
                        object->values[i] = NULLOBJ;
                    break;
                }

                case OBJ_PLAYER_DEAD: {
                    object->depth = -FxOne;

                    object->values[VAL_PLAYER_INDEX] = -1L;
                    break;
                }

                case OBJ_KEVIN: {
                    object->depth = -FxOne;

                    object->bbox[0][0] = FfInt(-9L);
                    object->bbox[0][1] = FfInt(-25L);
                    object->bbox[1][0] = FfInt(10L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_KEVIN_PLAYER] = -1L;
                    break;
                }

                case OBJ_PLAYER_EFFECT: {
                    object->values[VAL_PLAYER_EFFECT_POWER] = POW_SMALL;
                    object->values[VAL_PLAYER_EFFECT_FRAME] = PF_IDLE;
                    object->values[VAL_PLAYER_EFFECT_ALPHA] = FfInt(255L);
                    break;
                }

                case OBJ_COIN:
                case OBJ_PSWITCH_COIN: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(6L);
                    object->bbox[0][1] = FfInt(2L);
                    object->bbox[1][0] = FfInt(25L);
                    object->bbox[1][1] = FfInt(30L);
                    break;
                }

                case OBJ_MUSHROOM:
                case OBJ_MUSHROOM_1UP: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(15L);

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_MUSHROOM_POISON: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(15L);

                    object->values[VAL_X_SPEED] = FfInt(2L);
                    break;
                }

                case OBJ_FIRE_FLOWER: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-17L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(16L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_BEETROOT: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_LUI: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-30L);
                    object->bbox[1][0] = FfInt(15L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_STARMAN: {
                    object->depth = FxOne;

                    object->bbox[0][0] = FfInt(-16L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(17L);
                    break;
                }

                case OBJ_POINTS: {
                    object->depth = FfInt(-100L);

                    object->values[VAL_POINTS_PLAYER] = -1L;
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    object->bbox[0][0] = object->bbox[0][1] = FfInt(-7L);
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(8L);

                    object->values[VAL_MISSILE_OWNER] = NULLOBJ;
                    break;
                }

                case OBJ_MISSILE_HAMMER: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-18L);
                    object->bbox[1][0] = FfInt(11L);
                    object->bbox[1][1] = FfInt(15L);

                    object->values[VAL_MISSILE_OWNER] = NULLOBJ;
                    break;
                }

                case OBJ_MISSILE_BEETROOT:
                case OBJ_MISSILE_BEETROOT_SINK: {
                    object->bbox[0][0] = FfInt(-11L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(12L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_MISSILE_OWNER] = NULLOBJ;
                    object->values[VAL_MISSILE_HITS] = 3L;
                    break;
                }

                case OBJ_ITEM_BLOCK:
                case OBJ_HIDDEN_BLOCK: {
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(32L);
                    object->depth = FfInt(20L);

                    object->values[VAL_BLOCK_ITEM] = OBJ_NULL;
                    break;
                }

                case OBJ_BRICK_BLOCK:
                case OBJ_PSWITCH_BRICK: {
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(32L);
                    object->depth = FfInt(20L);
                    break;
                }

                case OBJ_BRICK_BLOCK_COIN: {
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(32L);
                    object->depth = FfInt(20L);

                    object->values[VAL_BLOCK_ITEM] = OBJ_NULL;
                    break;
                }

                case OBJ_BLOCK_BUMP: {
                    object->bbox[1][0] = FfInt(32L);
                    object->bbox[0][1] = FfInt(-8L);

                    object->values[VAL_BLOCK_BUMP_OWNER] = NULLOBJ;
                    break;
                }

                case OBJ_BRICK_SHARD: {
                    object->bbox[0][0] = object->bbox[0][1] = FfInt(-8L);
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(8L);
                    break;
                }

                case OBJ_CHECKPOINT: {
                    object->bbox[0][0] = FfInt(-19L);
                    object->bbox[0][1] = FfInt(-110L);
                    object->bbox[1][0] = FfInt(21L);
                    object->bbox[1][1] = FxOne;
                    object->depth = FfInt(2L);
                    break;
                }

                case OBJ_ROTODISC_BALL: {
                    object->depth = FfInt(19L);
                    break;
                }

                case OBJ_ROTODISC: {
                    object->bbox[0][0] = object->bbox[0][1] = FxOne;
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(30L);

                    object->values[VAL_ROTODISC_OWNER] = NULLOBJ;
                    break;
                }

                case OBJ_WATER_SPLASH: {
                    object->depth = FfInt(-2L);
                    break;
                }

                case OBJ_BUBBLE: {
                    object->bbox[0][0] = FfInt(-4L);
                    object->bbox[0][1] = FfInt(-4L);
                    object->bbox[1][0] = FfInt(5L);
                    object->bbox[1][1] = FfInt(6L);
                    break;
                }

                case OBJ_GOAL_MARK: {
                    object->bbox[1][0] = FfInt(31L);
                    object->bbox[1][1] = FfInt(32L);
                    break;
                }

                case OBJ_GOAL_BAR:
                case OBJ_GOAL_BAR_FLY: {
                    object->bbox[0][0] = FfInt(-23L);
                    object->bbox[1][0] = FfInt(21L);
                    object->bbox[1][1] = FfInt(16L);
                    break;
                }

                case OBJ_PSWITCH: {
                    object->bbox[1][0] = FfInt(31L);
                    object->bbox[1][1] = FfInt(32L);
                    object->depth = FxOne;
                    break;
                }

                case OBJ_THWOMP: {
                    object->bbox[0][0] = FfInt(-27L);
                    object->bbox[0][1] = FfInt(-33L);
                    object->bbox[1][0] = FfInt(26L);
                    object->bbox[1][1] = FfInt(35L);
                    break;
                }
            }

            state.next_object = (ObjectID)((index + 1L) % MAX_OBJECTS);
            return index;
        }
        index = (ObjectID)((index + 1L) % MAX_OBJECTS);
    }
    return NULLOBJ;
}

void move_object(ObjectID index, const fvec2 pos) {
    struct GameObject* object = get_object(index);
    if (object == NULL)
        return;

    object->pos[0] = pos[0];
    object->pos[1] = pos[1];

    int32_t bx = object->pos[0] / BLOCK_SIZE;
    int32_t by = object->pos[1] / BLOCK_SIZE;
    bx = SDL_clamp(bx, 0L, MAX_BLOCKS - 1L);
    by = SDL_clamp(by, 0L, MAX_BLOCKS - 1L);

    int32_t block = (by * MAX_BLOCKS) + bx;
    if (block == object->block)
        return;

    if (object->block >= 0L && object->block < (int32_t)BLOCKMAP_SIZE) {
        if (object_is_alive(object->previous_block))
            state.objects[object->previous_block].next_block = object->next_block;
        if (object_is_alive(object->next_block))
            state.objects[object->next_block].previous_block = object->previous_block;
        if (state.blockmap[object->block] == index)
            state.blockmap[object->block] = object->previous_block;
    }

    object->block = block;
    object->previous_block = state.blockmap[block];
    object->next_block = NULLOBJ;
    if (object_is_alive(state.blockmap[block]))
        state.objects[state.blockmap[block]].next_block = index;
    state.blockmap[block] = index;
}

void list_block_at(struct BlockList* list, const fvec2 rect[2]) {
    list->num_objects = 0;

    int32_t bx1 = Fsub(rect[0][0], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by1 = Fsub(rect[0][1], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t bx2 = Fadd(rect[1][0], BLOCK_SIZE) / BLOCK_SIZE;
    int32_t by2 = Fadd(rect[1][1], BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);

    for (int32_t by = by1; by <= by2; by++)
        for (int32_t bx = bx1; bx <= bx2; bx++) {
            ObjectID oid = state.blockmap[bx + (by * MAX_BLOCKS)];
            while (object_is_alive(oid)) {
                const struct GameObject* other = &(state.objects[oid]);
                if (!(other->flags & FLG_DESTROY)) {
                    const fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                    const fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                    const fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                    const fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);

                    if (rect[0][0] < ox2 && rect[1][0] > ox1 && rect[0][1] < oy2 && rect[1][1] > oy1)
                        list->objects[(list->num_objects)++] = oid;
                }
                oid = other->previous_block;
            }
        }
}

void kill_object(ObjectID index) {
    struct GameObject* object = get_object(index);
    if (object != NULL)
        object->flags |= FLG_DESTROY;
}

void destroy_object(ObjectID index) {
    struct GameObject* object = get_object(index);
    if (object == NULL)
        return;

    switch (object->type) {
        default:
            break;

        case OBJ_PLAYER_SPAWN: {
            if (state.spawn == index)
                state.spawn = NULLOBJ;
            break;
        }

        case OBJ_PLAYER: {
            struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_PLAYER_INDEX]));
            if (player != NULL && player->object == index)
                player->object = NULLOBJ;

            for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++) {
                struct GameObject* missile = get_object((ObjectID)(object->values[i]));
                if (missile != NULL)
                    missile->values[VAL_MISSILE_OWNER] = NULLOBJ;
            }

            for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++) {
                struct GameObject* beetroot = get_object((ObjectID)(object->values[i]));
                if (beetroot != NULL)
                    beetroot->values[VAL_MISSILE_OWNER] = NULLOBJ;
            }

            break;
        }

        case OBJ_KEVIN: {
            struct GamePlayer* player = get_player((PlayerID)(object->values[VAL_PLAYER_INDEX]));
            if (player != NULL && player->kevin.object == index)
                player->kevin.object = NULLOBJ;
            break;
        }

        case OBJ_MISSILE_FIREBALL:
        case OBJ_MISSILE_BEETROOT:
        case OBJ_MISSILE_HAMMER: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_MISSILE_OWNER]));
            if (owner != NULL && owner->type == OBJ_PLAYER)
                for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                    if (owner->values[i] == index) {
                        owner->values[i] = NULLOBJ;
                        break;
                    }
            break;
        }

        case OBJ_MISSILE_BEETROOT_SINK: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_MISSILE_OWNER]));
            if (owner != NULL && owner->type == OBJ_PLAYER)
                for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++)
                    if (owner->values[i] == index) {
                        owner->values[i] = NULLOBJ;
                        break;
                    }
            break;
        }

        case OBJ_CHECKPOINT: {
            if (state.checkpoint == index)
                state.checkpoint = NULLOBJ;
            break;
        }

        case OBJ_ROTODISC_BALL: {
            kill_object((ObjectID)(object->values[VAL_ROTODISC]));
            break;
        }

        case OBJ_ROTODISC: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_ROTODISC_OWNER]));
            if (owner != NULL && owner->type == OBJ_ROTODISC_BALL)
                owner->values[VAL_ROTODISC] = NULLOBJ;
            break;
        }

        case OBJ_AUTOSCROLL: {
            if (state.autoscroll == index)
                state.autoscroll = NULLOBJ;
            break;
        }
    }
    object->type = OBJ_NULL;

    int32_t block = object->block;
    if (block >= 0L && block < (int32_t)BLOCKMAP_SIZE) {
        if (object_is_alive(object->previous_block))
            state.objects[object->previous_block].next_block = object->next_block;
        if (object_is_alive(object->next_block))
            state.objects[object->next_block].previous_block = object->previous_block;
        if (state.blockmap[block] == index)
            state.blockmap[block] = object->previous_block;
    }

    if (state.live_objects == index)
        state.live_objects = object->previous;
    if (object_is_alive(object->previous))
        state.objects[object->previous].next = object->next;
    if (object_is_alive(object->next))
        state.objects[object->next].previous = object->previous;
}

void draw_object(ObjectID oid, const char* tid, GLfloat angle, const GLubyte color[4]) {
    const struct GameObject* object = &(state.objects[oid]);
    const struct InterpObject* smooth = &(interp[oid]);
    draw_sprite(
        tid,
        (float[3]){FtInt(smooth->pos[0]), FtInt(smooth->pos[1]) + object->values[VAL_SPROUT],
                   (object->values[VAL_SPROUT] > 0L) ? 21 : FtFloat(object->depth)},
        (bool[2]){object->flags & FLG_X_FLIP, object->flags & FLG_Y_FLIP}, angle, color
    );
}

void play_sound_at_object(struct GameObject* object, const char* sid) {
    play_sound_at(sid, FtFloat(object->pos[0]), FtFloat(object->pos[1]));
}

int32_t random() {
    // https://rosettacode.org/wiki/Linear_congruential_generator
    state.seed = (state.seed * 214013 + 2531011) & ((1U << 31) - 1);
    return (int32_t)state.seed >> 16;
}

void interp_start() {
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        interp[oid].from[0] = interp[oid].to[0];
        interp[oid].from[1] = interp[oid].to[1];
        oid = state.objects[oid].previous;
    }
}

void interp_end() {
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        const struct GameObject* object = &(state.objects[oid]);
        interp[oid].to[0] = object->pos[0];
        interp[oid].to[1] = object->pos[1];
        oid = object->previous;
    }
}

void interp_update(float ticfrac) {
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        const struct GameObject* object = &(state.objects[oid]);
        struct InterpObject* smooth = &(interp[oid]);

        if (smooth->type != object->type) { // Game state mismatch, skip interpolating this object
            smooth->type = object->type;
            smooth->from[0] = smooth->to[0] = smooth->pos[0] = object->pos[0];
            smooth->from[1] = smooth->to[1] = smooth->pos[1] = object->pos[1];
        } else {
            smooth->pos[0] = (fix16_t)((float)(smooth->from[0]) + ((float)(smooth->to[0] - smooth->from[0]) * ticfrac));
            smooth->pos[1] = (fix16_t)((float)(smooth->from[1]) + ((float)(smooth->to[1] - smooth->from[1]) * ticfrac));
        }

        oid = object->previous;
    }

    const ObjectID asid = state.autoscroll;
    if (object_is_alive(asid)) {
        float cx = FtInt(interp[asid].pos[0]);
        float cy = FtInt(interp[asid].pos[1]);
        const float quake = get_quake();
        if (quake > 0) {
            cx += (SDL_randf() - SDL_randf()) * quake;
            cy += (SDL_randf() - SDL_randf()) * quake;
        }

        const float bx1 = FtFloat(state.bounds[0][0]) + HALF_SCREEN_WIDTH;
        const float by1 = FtFloat(state.bounds[0][1]) + HALF_SCREEN_HEIGHT;
        const float bx2 = FtFloat(state.bounds[1][0]) - HALF_SCREEN_WIDTH;
        const float by2 = FtFloat(state.bounds[1][1]) - HALF_SCREEN_HEIGHT;
        move_camera(SDL_clamp(cx, bx1, bx2), SDL_clamp(cy, by1, by2));
    } else {
        const struct GamePlayer* player = get_player(local_player);
        if (player != NULL) {
            const ObjectID pwid = player->object;
            if (object_is_alive(pwid)) {
                float cx = FtInt(interp[pwid].pos[0]);
                float cy = FtInt(interp[pwid].pos[1]);
                const float quake = get_quake();
                if (quake > 0) {
                    cx += (SDL_randf() - SDL_randf()) * quake;
                    cy += (SDL_randf() - SDL_randf()) * quake;
                }

                const float bx1 = FtFloat(player->bounds[0][0]) + HALF_SCREEN_WIDTH;
                const float by1 = FtFloat(player->bounds[0][1]) + HALF_SCREEN_HEIGHT;
                const float bx2 = FtFloat(player->bounds[1][0]) - HALF_SCREEN_WIDTH;
                const float by2 = FtFloat(player->bounds[1][1]) - HALF_SCREEN_HEIGHT;
                move_camera(SDL_clamp(cx, bx1, bx2), SDL_clamp(cy, by1, by2));
            }
        }
    }
}

void skip_interp(ObjectID oid) {
    const struct GameObject* object = get_object(oid);
    if (object == NULL)
        return;

    struct InterpObject* smooth = &(interp[oid]);
    smooth->type = object->type;
    smooth->from[0] = smooth->to[0] = smooth->pos[0] = object->pos[0];
    smooth->from[1] = smooth->to[1] = smooth->pos[1] = object->pos[1];
}
