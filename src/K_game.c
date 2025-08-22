#include "K_game.h"
#include "K_audio.h"
#include "K_log.h"

static struct GameState state = {0};
static int local_player = -1L;

static struct InterpObject interp[MAX_OBJECTS] = {0};

/* ====

   GAME

   ==== */

static ObjectID find_object(enum GameObjectType type) {
    ObjectID oid = state.live_objects;
    while (oid >= 0L) {
        const struct GameObject* object = &(state.objects[oid]);
        if (object->type == type)
            return oid;
        oid = object->previous;
    }
    return -1L;
}

static struct GamePlayer* get_player(int pid) {
    return (pid < 0L || pid >= MAX_PLAYERS) ? NULL : &(state.players[pid]);
}

static ObjectID respawn_player(int pid) {
    struct GamePlayer* player = get_player(pid);
    if (player == NULL || !(player->active) || player->lives <= 0L || state.clock == 0L ||
        state.sequence.type != SEQ_NONE)
        return -1L;

    kill_object(player->object);
    player->object = -1L;

    const struct GameObject* spawn = get_object(state.checkpoint);
    if (spawn == NULL) {
        spawn = get_object(state.spawn);
        if (spawn == NULL)
            return -1L;
    }

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

    const ObjectID object = create_object(OBJ_PLAYER, spawn->pos);
    if (object_is_alive(object))
        state.objects[object].values[VAL_PLAYER_INDEX] = pid;
    player->object = object;
    return object;
}

static void give_points(struct GameObject* item, int pid, int points) {
    struct GamePlayer* player = get_player(pid);
    if (player == NULL)
        return;

    if (points < 0L) {
        player->lives -= points;
        if (pid == local_player)
            play_sound(SND_1UP);
    } else
        player->score += points;

    struct GameObject* pts =
        get_object(create_object(OBJ_POINTS, (fvec2){item->pos[0], Fadd(item->pos[1], item->bbox[0][1])}));
    if (pts != NULL) {
        pts->values[VAL_POINTS_PLAYER] = pid;
        pts->values[VAL_POINTS] = points;
    }
}

static bool is_solid(ObjectID oid, bool ignore_full, bool ignore_top) {
    switch (state.objects[oid].type) {
        case OBJ_SOLID:
        case OBJ_ITEM_BLOCK:
        case OBJ_BRICK_BLOCK:
        case OBJ_BRICK_BLOCK_COIN:
        case OBJ_PSWITCH_BRICK:
            return !ignore_full;
        case OBJ_SOLID_TOP:
            return !ignore_top;
        default:
            return false;
    }
}

static bool touching_solid(const fvec2 rect[2]) {
    int bx1 = Fsub(rect[0][0], BLOCK_SIZE) / BLOCK_SIZE;
    int by1 = Fsub(rect[0][1], BLOCK_SIZE) / BLOCK_SIZE;
    int bx2 = Fadd(rect[1][0], BLOCK_SIZE) / BLOCK_SIZE;
    int by2 = Fadd(rect[1][1], BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);
    for (int by = by1; by <= by2; by++)
        for (int bx = bx1; bx <= bx2; bx++) {
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

static int get_owner_id(ObjectID oid) {
    const struct GameObject* object = get_object(oid);
    if (object == NULL)
        return -1;

    switch (object->type) {
        default:
            return -1;

        case OBJ_PLAYER:
        case OBJ_PLAYER_DEAD:
            return object->values[VAL_PLAYER_INDEX];

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
            return object->values[VAL_POINTS_PLAYER];

        case OBJ_ROTODISC:
            return get_owner_id((ObjectID)(object->values[VAL_ROTODISC_OWNER]));
    }
}

static struct GamePlayer* get_owner(ObjectID oid) {
    return get_player(get_owner_id(oid));
}

static void bump_block(struct GameObject* block, ObjectID from, bool strong) {
    if ((block->flags & FLG_BLOCK_EMPTY) ||
        (object_is_alive(from) && state.objects[from].type == OBJ_PLAYER && block->values[VAL_BLOCK_BUMP] > 0L))
        return;

    switch (block->type) {
        default:
            break;

        case OBJ_ITEM_BLOCK:
        case OBJ_BRICK_BLOCK_COIN: {
            bool is_powerup = false;
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
                    iid,
                    (fvec2){Fadd(block->pos[0], Flerp(block->bbox[0][0], block->bbox[1][0], FxHalf)), block->pos[1]}
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
                    play_sound_at_object(item, SND_SPROUT);
                }
            }

            block->values[VAL_BLOCK_BUMP] = 1L;
            if (block->type == OBJ_ITEM_BLOCK)
                block->flags |= FLG_BLOCK_EMPTY;
            else {
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
                }
                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(16)), Fadd(block->pos[1], FfInt(8))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(2);
                    shard->values[VAL_Y_SPEED] = FfInt(-8);
                }
                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(8)), Fadd(block->pos[1], FfInt(16))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(-3);
                    shard->values[VAL_Y_SPEED] = FfInt(-6);
                }
                shard = get_object(create_object(
                    OBJ_BRICK_SHARD, (fvec2){Fadd(block->pos[0], FfInt(16)), Fadd(block->pos[1], FfInt(16))}
                ));
                if (shard != NULL) {
                    shard->values[VAL_X_SPEED] = FfInt(3);
                    shard->values[VAL_Y_SPEED] = FfInt(-6);
                }

                struct GamePlayer* player = get_owner(from);
                if (player != NULL)
                    player->score += 50L;

                play_sound_at_object(block, SND_BREAK);
                block->flags |= FLG_DESTROY;
            } else if (block->values[VAL_BLOCK_BUMP] <= 0L) {
                block->values[VAL_BLOCK_BUMP] = 1L;
                play_sound_at_object(block, SND_BUMP);
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

static void displace_object(ObjectID did, fix16_t climb, bool unstuck) {
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
    bool climbed = false;

    if (list.num_objects > 0L) {
        bool stop = false;
        if (displacee->values[VAL_X_SPEED] < FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, true))
                    continue;
                const struct GameObject* object = &(state.objects[oid]);

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
        bool stop = false;
        if (displacee->values[VAL_Y_SPEED] < FxZero) {
            for (size_t i = 0; i < list.num_objects; i++) {
                const ObjectID oid = list.objects[i];
                if (!is_solid(oid, false, true))
                    continue;
                const struct GameObject* object = &(state.objects[oid]);

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
                if (is_solid(oid, true, false) && Fsub(Fadd(y, displacee->bbox[1][1]), displacee->values[VAL_Y_SPEED]) >
                                                      Fadd(Fadd(object->pos[1], object->bbox[0][1]), climb))
                    continue;

                y = Fmin(y, Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]));
                stop = true;
                top_check(oid, did);
            }
            displacee->values[VAL_Y_TOUCH] = stop;
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
    state.sequence.activator = pawn->values[VAL_PLAYER_INDEX];

    const int pid = pawn->values[VAL_PLAYER_INDEX];
    for (size_t i = 0; i < MAX_PLAYERS; i++)
        if (i != pid) {
            kill_object(state.players[i].object);
            state.players[i].object = -1L;
        }

    for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++) {
        struct GameObject* missile = get_object((ObjectID)(pawn->values[i]));
        if (missile != NULL) {
            int points;
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
    play_track(TS_FANFARE, (state.flags & GF_LOST) ? MUS_WIN2 : MUS_WIN1, false);
}

static void kill_player(struct GameObject* pawn) {
    struct GameObject* dead = get_object(create_object(OBJ_PLAYER_DEAD, pawn->pos));
    if (dead != NULL) {
        // !!! CLIENT-SIDE !!!
        if (pawn->values[VAL_PLAYER_INDEX] == local_player && pawn->values[VAL_PLAYER_STARMAN] > 0L)
            stop_track(TS_POWER);
        // !!! CLIENT-SIDE !!!

        struct GamePlayer* player =
            get_player((ObjectID)(dead->values[VAL_PLAYER_INDEX] = pawn->values[VAL_PLAYER_INDEX]));
        if (player != NULL)
            player->power = POW_SMALL;

        bool all_dead = true;
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

static bool hit_player(struct GameObject* pawn) {
    if (state.sequence.type == SEQ_WIN || pawn->values[VAL_PLAYER_FLASH] > 0L || pawn->values[VAL_PLAYER_STARMAN] > 0L)
        return false;

    struct GamePlayer* player = get_player(pawn->values[VAL_PLAYER_INDEX]);
    if (player != NULL) {
        if (player->power == POW_SMALL) {
            kill_player(pawn);
            return true;
        }
        player->power = (player->power == POW_BIG) ? POW_SMALL : POW_BIG;
    }

    pawn->values[VAL_PLAYER_POWER] = 3000L;
    pawn->values[VAL_PLAYER_FLASH] = 100L;
    play_sound_at_object(pawn, SND_WARP);
    return true;
}

static bool bump_check(ObjectID self_id, ObjectID other_id) {
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
                play_sound_at_object(other, SND_STOMP);
            } else if (Fabs(self->values[VAL_X_SPEED]) > Fabs(other->values[VAL_X_SPEED])) {
                if ((self->values[VAL_X_SPEED] > FxZero && self->pos[0] < other->pos[0]) ||
                    (self->values[VAL_X_SPEED] < FxZero && self->pos[0] > other->pos[0])) {
                    other->values[VAL_X_SPEED] = Fadd(other->values[VAL_X_SPEED], Fhalf(self->values[VAL_X_SPEED]));
                    self->values[VAL_X_SPEED] = -Fhalf(self->values[VAL_X_SPEED]);
                    if (Fabs(self->values[VAL_X_SPEED]) >= FfInt(2L))
                        play_sound_at_object(self, SND_BUMP);
                }
            }
            break;
        }

        case OBJ_COIN:
        case OBJ_PSWITCH_COIN: {
            const struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;
            const int pid = get_owner_id(other_id);
            struct GamePlayer* player = get_player(pid);
            if (player == NULL)
                break;

            if (++(player->coins) >= 100L) {
                give_points(self, pid, -1L);
                player->coins -= 100L;
            }

            player->score += 200L;
            play_sound_at_object(self, SND_COIN);
            self->flags |= FLG_DESTROY;
            break;
        }

        case OBJ_MUSHROOM: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = get_owner_id(other_id);
                struct GamePlayer* player = get_player(pid);
                if (player == NULL)
                    break;

                if (player->power == POW_SMALL) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_MUSHROOM_1UP: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                give_points(self, get_owner_id(other_id), -1L);
                self->flags |= FLG_DESTROY;
            }
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
            if (other->type == OBJ_PLAYER) {
                const int pid = get_owner_id(other_id);
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

                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_BEETROOT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = get_owner_id(other_id);
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

                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_LUI: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = get_owner_id(other_id);
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

                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_HAMMER_SUIT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = get_owner_id(other_id);
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

                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_STARMAN: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                // !!! CLIENT-SIDE !!!
                if (other->values[VAL_PLAYER_INDEX] == local_player)
                    play_track(TS_POWER, MUS_STARMAN, true);
                // !!! CLIENT-SIDE !!!
                other->values[VAL_PLAYER_STARMAN] = 500L;
                play_sound_at_object(other, SND_GROW);
                self->flags |= FLG_DESTROY;
            }
            break;
        }

        case OBJ_CHECKPOINT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type != OBJ_PLAYER)
                break;
            if (state.checkpoint < self_id) {
                const int pid = get_owner_id(other_id);
                give_points(self, pid, 1000L);

                struct GamePlayer* player = get_player(pid);
                if (player != NULL &&
                    self->values[VAL_CHECKPOINT_BOUNDS_X1] != self->values[VAL_CHECKPOINT_BOUNDS_X2] &&
                    self->values[VAL_CHECKPOINT_BOUNDS_Y1] != self->values[VAL_CHECKPOINT_BOUNDS_Y2]) {
                    player->bounds[0][0] = self->values[VAL_CHECKPOINT_BOUNDS_X1];
                    player->bounds[0][1] = self->values[VAL_CHECKPOINT_BOUNDS_Y1];
                    player->bounds[1][0] = self->values[VAL_CHECKPOINT_BOUNDS_X2];
                    player->bounds[1][1] = self->values[VAL_CHECKPOINT_BOUNDS_Y2];
                }

                play_sound_at_object(self, SND_SPROUT);
                state.checkpoint = self_id;
            }
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

            int points;
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
                    other->values[VAL_Y_SPEED] = FxZero;
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

            play_sound_at_object(self, SND_TOGGLE);
            play_track(TS_SWITCH, MUS_PSWITCH, true);
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

    int bx1 = Fsub(x1, BLOCK_SIZE) / BLOCK_SIZE;
    int by1 = Fsub(y1, BLOCK_SIZE) / BLOCK_SIZE;
    int bx2 = Fadd(x2, BLOCK_SIZE) / BLOCK_SIZE;
    int by2 = Fadd(y2, BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);
    for (int by = by1; by <= by2; by++)
        for (int bx = bx1; bx <= bx2; bx++) {
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

static enum PlayerFrames get_player_frame(const struct GameObject* object) {
    if (object->values[VAL_PLAYER_POWER] > 0L) {
        const struct GamePlayer* player = get_player(object->values[VAL_PLAYER_INDEX]);
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
            const int frame = FtInt(object->values[VAL_PLAYER_FRAME]);
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

static enum TextureIndices get_player_texture(enum PlayerPowers power, enum PlayerFrames frame) {
    switch (power) {
        default:
        case POW_SMALL: {
            switch (frame) {
                default:
                    return TEX_MARIO_SMALL;

                case PF_WALK1:
                    return TEX_MARIO_SMALL_WALK1;
                case PF_WALK2:
                    return TEX_MARIO_SMALL_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_SMALL_JUMP;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_SMALL_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_SMALL_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_SMALL_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_SMALL_SWIM4;

                case PF_GROW2:
                    return TEX_MARIO_BIG_GROW;
                case PF_GROW3:
                    return TEX_MARIO_BIG_WALK2;
            }
            break;
        }

        case POW_BIG: {
            switch (frame) {
                default:
                    return TEX_MARIO_BIG;

                case PF_WALK1:
                    return TEX_MARIO_BIG_WALK1;
                case PF_WALK2:
                case PF_GROW1:
                    return TEX_MARIO_BIG_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_BIG_JUMP;

                case PF_DUCK:
                    return TEX_MARIO_BIG_DUCK;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_BIG_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_BIG_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_BIG_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_BIG_SWIM4;

                case PF_GROW2:
                    return TEX_MARIO_BIG_GROW;
                case PF_GROW3:
                    return TEX_MARIO_SMALL;
            }
            break;
        }

        case POW_FIRE: {
            switch (frame) {
                default:
                    return TEX_MARIO_FIRE;

                case PF_WALK1:
                    return TEX_MARIO_FIRE_WALK1;
                case PF_WALK2:
                case PF_GROW3:
                    return TEX_MARIO_FIRE_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_FIRE_JUMP;

                case PF_DUCK:
                    return TEX_MARIO_FIRE_DUCK;

                case PF_FIRE:
                    return TEX_MARIO_FIRE_FIRE;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_FIRE_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_FIRE_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_FIRE_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_FIRE_SWIM4;

                case PF_GROW1:
                    return TEX_MARIO_BIG_WALK2;
                case PF_GROW2:
                    return TEX_MARIO_FIRE_GROW1;
                case PF_GROW4:
                    return TEX_MARIO_FIRE_GROW2;
            }
            break;
        }

        case POW_BEETROOT: {
            switch (frame) {
                default:
                    return TEX_MARIO_BEETROOT;

                case PF_WALK1:
                    return TEX_MARIO_BEETROOT_WALK1;
                case PF_WALK2:
                case PF_GROW3:
                    return TEX_MARIO_BEETROOT_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_BEETROOT_JUMP;

                case PF_DUCK:
                    return TEX_MARIO_BEETROOT_DUCK;

                case PF_FIRE:
                    return TEX_MARIO_BEETROOT_FIRE;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_BEETROOT_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_BEETROOT_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_BEETROOT_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_BEETROOT_SWIM4;

                case PF_GROW1:
                    return TEX_MARIO_BIG_WALK2;
                case PF_GROW2:
                    return TEX_MARIO_FIRE_GROW1;
                case PF_GROW4:
                    return TEX_MARIO_FIRE_GROW2;
            }
            break;
        }

        case POW_LUI: {
            switch (frame) {
                default:
                    return TEX_MARIO_LUI;

                case PF_WALK1:
                    return TEX_MARIO_LUI_WALK1;
                case PF_WALK2:
                case PF_GROW3:
                    return TEX_MARIO_LUI_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_LUI_JUMP;

                case PF_DUCK:
                    return TEX_MARIO_LUI_DUCK;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_LUI_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_LUI_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_LUI_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_LUI_SWIM4;

                case PF_GROW1:
                    return TEX_MARIO_BIG_WALK2;
                case PF_GROW2:
                    return TEX_MARIO_FIRE_GROW1;
                case PF_GROW4:
                    return TEX_MARIO_FIRE_GROW2;
            }
            break;
        }

        case POW_HAMMER: {
            switch (frame) {
                default:
                    return TEX_MARIO_HAMMER;

                case PF_WALK1:
                    return TEX_MARIO_HAMMER_WALK1;
                case PF_WALK2:
                case PF_GROW3:
                    return TEX_MARIO_HAMMER_WALK2;

                case PF_JUMP:
                case PF_FALL:
                    return TEX_MARIO_HAMMER_JUMP;

                case PF_DUCK:
                    return TEX_MARIO_HAMMER_DUCK;

                case PF_FIRE:
                    return TEX_MARIO_HAMMER_FIRE;

                case PF_SWIM1:
                case PF_SWIM4:
                    return TEX_MARIO_HAMMER_SWIM1;
                case PF_SWIM2:
                    return TEX_MARIO_HAMMER_SWIM2;
                case PF_SWIM3:
                    return TEX_MARIO_HAMMER_SWIM3;
                case PF_SWIM5:
                    return TEX_MARIO_HAMMER_SWIM4;

                case PF_GROW1:
                    return TEX_MARIO_BIG_WALK2;
                case PF_GROW2:
                    return TEX_MARIO_FIRE_GROW1;
                case PF_GROW4:
                    return TEX_MARIO_FIRE_GROW2;
            }
            break;
        }
    }

    return TEX_MARIO_SMALL;
}

static bool in_any_view(struct GameObject* object, fix16_t padding, bool ignore_top) {
    const fix16_t sx1 = Fadd(object->pos[0], object->bbox[0][0]);
    const fix16_t sy1 = Fadd(object->pos[1], object->bbox[0][1]);
    const fix16_t sx2 = Fadd(object->pos[0], object->bbox[1][0]);
    const fix16_t sy2 = Fadd(object->pos[1], object->bbox[1][1]);

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

static bool in_player_view(struct GameObject* object, int pid, fix16_t padding, bool ignore_top) {
    const struct GamePlayer* player = get_player(pid);
    if (player == NULL || !(player->active) || !object_is_alive(player->object))
        return false;

    const fix16_t sx1 = Fadd(object->pos[0], object->bbox[0][0]);
    const fix16_t sy1 = Fadd(object->pos[1], object->bbox[0][1]);
    const fix16_t sx2 = Fadd(object->pos[0], object->bbox[1][0]);
    const fix16_t sy2 = Fadd(object->pos[1], object->bbox[1][1]);

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

static bool below_frame(struct GameObject* object) {
    return Fadd(object->pos[1], object->bbox[0][1]) > state.size[1];
}

/* ======

   ENGINE

   ====== */

void start_state(int num_players, int local) {
    local_player = local;

    SDL_memset(&state, 0, sizeof(state));
    clear_tiles();
    state.live_objects = -1L;
    for (uint32_t i = 0L; i < BLOCKMAP_SIZE; i++)
        state.blockmap[i] = -1L;

    state.size[0] = FfInt(2560L);
    state.size[1] = F_SCREEN_HEIGHT;
    state.bounds[1][0] = FfInt(2560L);
    state.bounds[1][1] = F_SCREEN_HEIGHT;
    state.track = MUS_SNOW;

    state.spawn = state.checkpoint = -1L;
    state.water = FfInt(240L); // 0x7FFFFFFF;

    state.flags |= GF_HARDCORE | GF_LOST;
    state.clock = 360L;

    state.sequence.activator = -1L;

    //
    //
    //
    // DATA LOADER
    //
    //
    //
    load_texture(TEX_WATER1);
    load_texture(TEX_WATER2);
    load_texture(TEX_WATER3);
    load_texture(TEX_WATER4);
    load_texture(TEX_WATER5);
    load_texture(TEX_WATER6);
    load_texture(TEX_WATER7);
    load_texture(TEX_WATER8);

    load_font(FNT_HUD);

    load_sound(SND_HURRY);
    load_sound(SND_TICK);

    if (state.track != MUS_NULL)
        load_track(state.track);

    //
    //
    //
    // LEVEL LOADER
    //
    //
    //
    add_gradient(
        0, 0, 11008, 551, 200,
        (GLubyte[4][4]){{192, 192, 192, 255}, {192, 192, 192, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}}
    );
    add_backdrop(TEX_SNOW1, 0, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 32, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 64, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 96, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 128, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 160, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 192, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 224, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 256, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 288, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 320, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 352, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 384, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 416, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 448, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 480, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 512, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 544, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 576, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW3, 608, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW4, 0, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 32, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 64, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 96, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 128, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 160, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 192, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 224, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 256, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 288, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 320, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 352, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 384, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 416, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 448, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 480, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 512, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 544, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 576, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW6, 608, 448, 20, 255, 255, 255, 255);

    for (int i = 0L; i < 30L; i++) {
        const int x = 640L + (i * 32L);
        add_backdrop(TEX_BLOCK3, (GLfloat)x, 416, 20, 255, 255, 255, 255);
        const ObjectID oid = create_object(OBJ_SOLID, (fvec2){FfInt(x), FfInt(416L)});
        if (object_is_alive(oid)) {
            state.objects[oid].bbox[1][0] = FfInt(32L);
            state.objects[oid].bbox[1][1] = FfInt(32L);
        }
    }

    add_backdrop(TEX_BLOCK3, 608, 352, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BLOCK3, 608, 384, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BLOCK3, 576, 384, 20, 255, 255, 255, 255);

    add_backdrop(TEX_BRIDGE2, 32, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 64, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 96, 240, 20, 255, 255, 255, 255);

    add_backdrop(TEX_SNOW1, 1920, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 1952, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 1984, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2016, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2048, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2080, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2112, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2144, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2176, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2208, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2240, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2272, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2304, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2336, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2368, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2400, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2432, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2464, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2496, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2528, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW2, 2560, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW4, 1920, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 1952, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 1984, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2016, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2048, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2080, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2112, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2144, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2176, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2208, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2240, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2272, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2304, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2336, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2368, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2400, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2432, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2464, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2496, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2528, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_SNOW5, 2560, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GOAL_MARK, 2144, 384, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GOAL, 2304, 128, 20, 255, 255, 255, 255);

    create_object(OBJ_CLOUD, (fvec2){FfInt(128L), FfInt(32L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(213L), FfInt(56L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(431L), FfInt(93L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(512L), FfInt(47L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(679L), FfInt(78L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(744L), FfInt(60L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(820L), FfInt(39L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(984L), FfInt(49L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(1184L), FfInt(87L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(789L), FfInt(96L)});
    create_object(OBJ_BUSH_SNOW, (fvec2){FfInt(384L), FfInt(384L)});

    create_object(OBJ_PLAYER_SPAWN, (fvec2){FfInt(80L), FfInt(240L)});
    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(32L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(32L)});
    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(64L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(64L)});
    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(96L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(96L)});
    create_object(OBJ_MUSHROOM, (fvec2){FfInt(256L), FfInt(416L)});
    create_object(OBJ_FIRE_FLOWER, (fvec2){FfInt(320L), FfInt(416L)});
    create_object(OBJ_BEETROOT, (fvec2){FfInt(384L), FfInt(416L)});
    create_object(OBJ_LUI, (fvec2){FfInt(448L), FfInt(416L)});
    create_object(OBJ_HAMMER_SUIT, (fvec2){FfInt(512L), FfInt(416L)});
    create_object(OBJ_MUSHROOM_1UP, (fvec2){FfInt(800L), FfInt(416L)});
    create_object(OBJ_BRICK_BLOCK, (fvec2){FfInt(160L), FfInt(240L)});
    create_object(OBJ_COIN, (fvec2){FfInt(160L), FfInt(208L)});
    create_object(OBJ_CHECKPOINT, (fvec2){FfInt(864L), FfInt(416L)});
    create_object(OBJ_CHECKPOINT, (fvec2){FfInt(1024L), FfInt(416L)});
    create_object(OBJ_GOAL_BAR, (fvec2){FfInt(2347L), FfInt(144L)});
    create_object(OBJ_PSWITCH, (fvec2){FfInt(128L), FfInt(208L)});

    struct GameObject* object = get_object(create_object(OBJ_ROTODISC_BALL, (fvec2){FfInt(608L), FfInt(352L)}));
    if (object != NULL) {
        // state.objects[oid].values[VAL_ROTODISC_LENGTH] = FfInt(150L);
        object->values[VAL_ROTODISC_SPEED] = -0x00001658;
        object->flags |= FLG_ROTODISC_FLOWER;
    }

    object = get_object(create_object(OBJ_HIDDEN_BLOCK, (fvec2){FfInt(160L), FfInt(128L)}));
    if (object != NULL)
        object->values[VAL_BLOCK_ITEM] = OBJ_STARMAN;
    object = get_object(create_object(OBJ_GOAL_MARK, (fvec2){FfInt(2176L), FfInt(384L)}));
    if (object != NULL) {
        object->bbox[1][0] = FfInt(128L);
        object->bbox[1][1] = FfInt(32L);
    }
    object = get_object(create_object(OBJ_GOAL_MARK, (fvec2){FfInt(2304L), FfInt(384L)}));
    if (object != NULL) {
        object->bbox[1][0] = FfInt(128L);
        object->bbox[1][1] = FfInt(32L);
    }
    object = get_object(create_object(OBJ_GOAL_MARK, (fvec2){FfInt(2432L), FfInt(384L)}));
    if (object != NULL) {
        object->bbox[1][0] = FfInt(128L);
        object->bbox[1][1] = FfInt(32L);
    }
    object = get_object(create_object(OBJ_GOAL_MARK, (fvec2){FfInt(2560L), FfInt(384L)}));
    if (object != NULL) {
        object->bbox[1][0] = FfInt(128L);
        object->bbox[1][1] = FfInt(32L);
    }

    object = get_object(create_object(OBJ_BRICK_BLOCK_COIN, (fvec2){FfInt(128L), FfInt(240L)}));
    if (object != NULL)
        object->values[VAL_BLOCK_ITEM] = OBJ_COIN_POP;
    object = get_object(create_object(OBJ_ITEM_BLOCK, (fvec2){FfInt(192L), FfInt(240L)}));
    if (object != NULL)
        object->values[VAL_BLOCK_ITEM] = OBJ_COIN_POP;
    object = get_object(create_object(OBJ_ITEM_BLOCK, (fvec2){FfInt(224L), FfInt(240L)}));
    if (object != NULL)
        object->values[VAL_BLOCK_ITEM] = OBJ_FIRE_FLOWER;

    ObjectID oid = create_object(OBJ_SOLID, (fvec2){FxZero, FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(640L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(128L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(256L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(384L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(512L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(-32L), FfInt(-64L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(192L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(-32L), FfInt(128L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(-32L), FfInt(256L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(-32L), FfInt(384L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID_TOP, (fvec2){FfInt(32L), FfInt(240L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(96L);
        state.objects[oid].bbox[1][1] = FfInt(16L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(608L), FfInt(352L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(64L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(576L), FfInt(384L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(32L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(1920L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2048L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2176L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2304L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2432L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2432L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2560L), FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(128L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2624L), FfInt(-64L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(192L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2624L), FfInt(128L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2624L), FfInt(256L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID, (fvec2){FfInt(2624L), FfInt(384L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(32L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }

    //
    //
    //
    // PLAYER START
    //
    //
    //
    for (size_t i = 0; i < num_players; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->active = true;

        player->lives = 4L;
        respawn_player((int)i);
    }

    if (state.track != MUS_NULL)
        play_track(TS_LEVEL, state.track, true);
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
    INFO("  Track: %u", state.track);
    INFO("  Time: %zu", state.time);
    INFO("  Seed: %u", state.seed);
    INFO("  Clock: %u", state.clock);
    INFO("  Spawn: %u", state.spawn);
    INFO("  Checkpoint: %u", state.checkpoint);
    INFO("  Scroll: (%.2f, %.2f)", FtDouble(state.scroll[0]), FtDouble(state.scroll[1]));
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

void tick_state(enum GameInput inputs[MAX_PLAYERS]) {
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->last_input = player->input;
        player->input = inputs[i];
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
                    struct GamePlayer* player = get_player(object->values[VAL_PLAYER_INDEX]);
                    if (player == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }
                    if (state.sequence.type == SEQ_WIN) {
                        player->input = GI_NONE;
                        object->values[VAL_X_SPEED] = 0x00028000;
                    }

                    const bool cant_run = !(player->input & GI_RUN) || object->pos[1] >= state.water;
                    const bool jumped = (player->input & GI_JUMP) && !(player->last_input & GI_JUMP);

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
                            play_sound_at_object(object, SND_SWIM);
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
                            play_sound_at_object(object, SND_JUMP);
                        }
                        if (!(player->input & GI_JUMP) && !(player->input & GI_DOWN) &&
                            object->values[VAL_PLAYER_GROUND] > 0L && (object->flags & FLG_PLAYER_JUMP))
                            object->flags &= ~FLG_PLAYER_JUMP;
                    }
                    if (object->pos[1] > state.water && (state.time % 5L) == 0L && (random() % 10L) == 5L)
                        create_object(
                            OBJ_BUBBLE,
                            (fvec2){object->pos[0], Fsub(object->pos[1], FfInt(player->power == POW_SMALL ? 18L : 39L))}
                        );

                    if (jumped && object->values[VAL_PLAYER_GROUND] <= 0L && object->values[VAL_Y_SPEED] > FxZero)
                        object->flags |= FLG_PLAYER_JUMP;

                    object->bbox[0][1] =
                        (player->power == POW_SMALL || (object->flags & FLG_PLAYER_DUCK)) ? FfInt(-25L) : FfInt(-51L);

                    if (object->values[VAL_PLAYER_GROUND] > 0L)
                        --(object->values[VAL_PLAYER_GROUND]);
                    displace_object(oid, FfInt(10L), true);
                    if (object->values[VAL_Y_TOUCH] > 0L)
                        object->values[VAL_PLAYER_GROUND] = 2L;

                    if (object->values[VAL_Y_TOUCH] <= 0L) {
                        const bool carried = object->flags & FLG_CARRIED;
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
                                        enum GameObjectType mtype;
                                        switch (player->power) {
                                            default:
                                                mtype = OBJ_NULL;
                                                break;
                                            case POW_FIRE:
                                                mtype = OBJ_MISSILE_FIREBALL;
                                                break;

                                            case POW_BEETROOT: {
                                                int beetroots = 0L;
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
                                                        (object->flags & FLG_X_FLIP) ? -0x0007C000 : 0x0007C000;
                                                    break;
                                                }

                                                case OBJ_MISSILE_BEETROOT: {
                                                    missile->values[VAL_X_SPEED] =
                                                        (object->flags & FLG_X_FLIP) ? -0x00026000 : 0x00026000;
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
                                            play_sound_at_object(object, SND_FIRE);
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
                        const int starman = --(object->values[VAL_PLAYER_STARMAN]);
                        if (starman == 100L)
                            play_sound_at_object(object, SND_STARMAN);
                        // !!! CLIENT-SIDE !!!
                        if (starman <= 0L && object->values[VAL_PLAYER_INDEX] == local_player)
                            stop_track(TS_POWER);
                        // !!! CLIENT-SIDE !!!
                    }

                    if (object->pos[1] > Fadd(player->bounds[1][1], FfInt(64L))) {
                        kill_player(object);
                        break;
                    }

                    bump_object(oid);
                    break;
                }

                case OBJ_PLAYER_DEAD: {
                    const int pid = object->values[VAL_PLAYER_INDEX];
                    struct GamePlayer* player = get_player(pid);
                    if (player == NULL) {
                        object->flags |= FLG_DESTROY;
                        break;
                    }

                    if (object->values[VAL_PLAYER_FRAME] >= 25L) {
                        object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                        move_object(oid, (fvec2){object->pos[0], Fadd(object->pos[1], object->values[VAL_Y_SPEED])});
                    }

                    switch ((object->values[VAL_PLAYER_FRAME])++) {
                        default:
                            break;

                        case 0: {
                            if (object->flags & FLG_PLAYER_DEAD_LAST) {
                                play_track(TS_FANFARE, (state.flags & GF_LOST) ? MUS_LOSE2 : MUS_LOSE1, false);
                                if (state.flags & GF_HARDCORE)
                                    play_sound(SND_HARDCORE);
                            } else {
                                play_sound_at_object(object, player->lives > 0L ? SND_LOSE : SND_DEAD);
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
                                play_sound_at_object(pawn, SND_RESPAWN);
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
                                play_track(TS_FANFARE, MUS_GAME_OVER, false);
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
                        play_sound_at_object(object, SND_KICK);
                    }

                    break;
                }

                case OBJ_POINTS: {
                    const int time = (object->values[VAL_POINTS_TIME])++;
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
                    if (object->flags & FLG_DESTROY) {
                        create_object(OBJ_EXPLODE, object->pos);
                        break;
                    }
                    if (object->values[VAL_Y_TOUCH] > 0L)
                        object->values[VAL_Y_SPEED] = FfInt(-5L);

                    const int player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
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

                    move_object(
                        oid, (fvec2){Fadd(object->pos[0], object->values[VAL_X_SPEED]),
                                     Fadd(object->pos[1], object->values[VAL_Y_SPEED])}
                    );

                    const int player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
                    if ((player >= 0L && !in_player_view(object, player, FxZero, true)) ||
                        (player < 0L && !in_any_view(object, FxZero, true)))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_MISSILE_BEETROOT: {
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                    move_object(
                        oid, (fvec2){Fadd(object->pos[0], object->values[VAL_X_SPEED]),
                                     Fadd(object->pos[1], object->values[VAL_Y_SPEED])}
                    );

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
                                play_sound_at_object(object, SND_HURT);
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

                    const int player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
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

                    const int player = get_owner_id((ObjectID)(object->values[VAL_MISSILE_OWNER]));
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
                    object->values[VAL_BRICK_SHARD_ANGLE] = Fadd(object->values[VAL_BRICK_SHARD_ANGLE], 0x00193333);

                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);
                    move_object(
                        oid, (fvec2){Fadd(object->pos[0], object->values[VAL_X_SPEED]),
                                     Fadd(object->pos[1], object->values[VAL_Y_SPEED])}
                    );

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
                        const int pid = get_owner_id((ObjectID)(object->values[VAL_COIN_POP_OWNER]));
                        struct GamePlayer* player = get_player(pid);
                        if (player != NULL) {
                            if (++(player->coins) >= 100L) {
                                give_points(object, pid, -1L);
                                player->coins -= 100L;
                            }
                            player->score += 200L;
                        }

                        play_sound_at_object(object, SND_COIN);
                        object->values[VAL_Y_SPEED] = -0x00044000;
                        object->flags |= FLG_COIN_POP_START;
                    }

                    if (object->values[VAL_Y_SPEED] < FxZero) {
                        move_object(oid, (fvec2){object->pos[0], Fadd(object->pos[1], object->values[VAL_Y_SPEED])});
                        object->values[VAL_Y_SPEED] = Fmin(Fadd(object->values[VAL_Y_SPEED], 0x00002AC1), FxZero);
                    }

                    if (object->flags & FLG_COIN_POP_SPARK) {
                        object->values[VAL_COIN_POP_FRAME] += 35L;
                        if (object->values[VAL_COIN_POP_FRAME] >= 400L) {
                            struct GameObject* points = get_object(create_object(OBJ_POINTS, object->pos));
                            if (points != NULL) {
                                points->values[VAL_POINTS_PLAYER] =
                                    get_owner_id((ObjectID)(object->values[VAL_COIN_POP_OWNER]));
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

                    int xoffs = random() % 2L;
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
                    move_object(oid, (fvec2){object->pos[0], Fadd(object->pos[1], object->values[VAL_Y_SPEED])});
                    break;
                }

                case OBJ_GOAL_BAR_FLY: {
                    object->values[VAL_GOAL_ANGLE] = Fadd(object->values[VAL_GOAL_ANGLE], 0x00006488);
                    object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00003333);
                    move_object(
                        oid, (fvec2){Fadd(object->pos[0], object->values[VAL_X_SPEED]),
                                     Fadd(object->pos[1], object->values[VAL_Y_SPEED])}
                    );
                    if (below_frame(object))
                        object->flags |= FLG_DESTROY;
                    break;
                }

                case OBJ_PSWITCH: {
                    if (object->values[VAL_PSWITCH] > 0L && !(object->flags & FLG_PSWITCH_ONCE))
                        --(object->values[VAL_PSWITCH]);
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
            play_sound(SND_STARMAN);
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
            play_sound(SND_HURRY);
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
            const int diff = (state.clock > 0L) + (((state.clock - 1L) >= 10L) * 10L);
            state.clock -= diff;

            struct GamePlayer* player = get_player(state.sequence.activator);
            if (player != NULL)
                player->score += diff * 10L;

            if ((state.time % 5L) == 0L)
                play_sound(SND_TICK);
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
                    enum TextureIndices tex;
                    switch (((int)((float)state.time / 12.5f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = TEX_CLOUD1;
                            break;
                        case 1:
                        case 3:
                            tex = TEX_CLOUD2;
                            break;
                        case 2:
                            tex = TEX_CLOUD3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUSH: {
                    enum TextureIndices tex;
                    switch (((int)((float)state.time / 7.142857142857143f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = TEX_BUSH1;
                            break;
                        case 1:
                        case 3:
                            tex = TEX_BUSH2;
                            break;
                        case 2:
                            tex = TEX_BUSH3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUSH_SNOW: {
                    enum TextureIndices tex;
                    switch (((int)((float)state.time / 7.142857142857143f) + object->values[VAL_PROP_FRAME]) % 4) {
                        default:
                            tex = TEX_BUSH_SNOW1;
                            break;
                        case 1:
                        case 3:
                            tex = TEX_BUSH_SNOW2;
                            break;
                        case 2:
                            tex = TEX_BUSH_SNOW3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_PLAYER: {
                    if (object->values[VAL_PLAYER_FLASH] > 0L && (state.time % 2))
                        break;

                    const struct GamePlayer* player = get_player(object->values[VAL_PLAYER_INDEX]);
                    const enum TextureIndices tex =
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
                    draw_object(
                        oid, TEX_MARIO_DEAD, 0, ALPHA(object->values[VAL_PLAYER_INDEX] != local_player ? 190 : 255)
                    );
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
                    enum TextureIndices tex;
                    switch ((state.time / 5) % 4) {
                        default:
                            tex = TEX_COIN1;
                            break;
                        case 1:
                        case 3:
                            tex = TEX_COIN2;
                            break;
                        case 2:
                            tex = TEX_COIN3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_COIN_POP: {
                    enum TextureIndices tex;
                    if (object->flags & FLG_COIN_POP_SPARK)
                        switch (object->values[VAL_COIN_POP_FRAME] / 100) {
                            default:
                                tex = TEX_COIN_SPARK1;
                                break;
                            case 1:
                                tex = TEX_COIN_SPARK2;
                                break;
                            case 2:
                                tex = TEX_COIN_SPARK3;
                                break;
                            case 3:
                                tex = TEX_COIN_SPARK4;
                                break;
                        }
                    else
                        switch ((object->values[VAL_COIN_POP_FRAME] / 100) % 5) {
                            default:
                                tex = TEX_COIN_POP1;
                                break;
                            case 2:
                                tex = TEX_COIN_POP2;
                                break;
                            case 3:
                                tex = TEX_COIN_POP3;
                                break;
                            case 4:
                                tex = TEX_COIN_POP4;
                                break;
                        }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM: {
                    draw_object(oid, TEX_MUSHROOM, 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM_1UP: {
                    draw_object(oid, TEX_MUSHROOM_1UP, 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM_POISON: {
                    draw_object(
                        oid,
                        ((int)((float)(state.time) / 11.11111111111111f) % 2) ? TEX_MUSHROOM_POISON2
                                                                              : TEX_MUSHROOM_POISON1,
                        0, WHITE
                    );
                    break;
                }

                case OBJ_FIRE_FLOWER: {
                    enum TextureIndices tex;
                    switch ((int)((float)state.time / 3.703703703703704f) % 4) {
                        default:
                            tex = TEX_FIRE_FLOWER1;
                            break;
                        case 1:
                            tex = TEX_FIRE_FLOWER2;
                            break;
                        case 2:
                            tex = TEX_FIRE_FLOWER3;
                            break;
                        case 3:
                            tex = TEX_FIRE_FLOWER4;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BEETROOT: {
                    enum TextureIndices tex;
                    switch ((int)((float)state.time / 12.5f) % 4) {
                        default:
                            tex = TEX_BEETROOT1;
                            break;
                        case 1:
                        case 3:
                            tex = TEX_BEETROOT2;
                            break;
                        case 2:
                            tex = TEX_BEETROOT3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_LUI: {
                    enum TextureIndices tex;
                    if (object->values[VAL_LUI_BOUNCE] > 0L)
                        switch (object->values[VAL_LUI_BOUNCE] / 100) {
                            default:
                            case 0:
                                tex = TEX_LUI2;
                                break;
                            case 1:
                            case 5:
                                tex = TEX_LUI_BOUNCE1;
                                break;
                            case 2:
                            case 4:
                                tex = TEX_LUI_BOUNCE2;
                                break;
                            case 3:
                                tex = TEX_LUI_BOUNCE3;
                                break;
                        }
                    else
                        switch (state.time % 12) {
                            default:
                            case 10:
                            case 11:
                                tex = TEX_LUI1;
                                break;
                            case 0:
                                tex = TEX_LUI2;
                                break;
                            case 1:
                            case 8:
                            case 9:
                                tex = TEX_LUI3;
                                break;
                            case 2:
                            case 6:
                            case 7:
                                tex = TEX_LUI4;
                                break;
                            case 3:
                            case 4:
                            case 5:
                                tex = TEX_LUI5;
                                break;
                        }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    draw_object(oid, TEX_HAMMER_SUIT, 0, WHITE);
                    break;
                }

                case OBJ_STARMAN: {
                    enum TextureIndices tex;
                    switch ((int)((float)state.time / 2.040816326530612f) % 4) {
                        default:
                            tex = TEX_STARMAN1;
                            break;
                        case 1:
                            tex = TEX_STARMAN2;
                            break;
                        case 2:
                            tex = TEX_STARMAN3;
                            break;
                        case 3:
                            tex = TEX_STARMAN4;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_POINTS: {
                    if (object->values[VAL_POINTS_PLAYER] != local_player)
                        break;

                    enum TextureIndices tex;
                    switch (object->values[VAL_POINTS]) {
                        default:
                        case 100:
                            tex = TEX_100;
                            break;
                        case 200:
                            tex = TEX_200;
                            break;
                        case 500:
                            tex = TEX_500;
                            break;
                        case 1000:
                            tex = TEX_1000;
                            break;
                        case 2000:
                            tex = TEX_2000;
                            break;
                        case 5000:
                            tex = TEX_5000;
                            break;
                        case 10000:
                            tex = TEX_10000;
                            break;
                        case 1000000:
                            tex = TEX_1000000;
                            break;
                        case -1:
                            tex = TEX_1UP;
                            break;
                    }

                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    draw_object(oid, TEX_MISSILE_FIREBALL, FtFloat(object->values[VAL_MISSILE_ANGLE]), WHITE);
                    break;
                }

                case OBJ_MISSILE_BEETROOT:
                case OBJ_MISSILE_BEETROOT_SINK: {
                    draw_object(oid, TEX_MISSILE_BEETROOT, 0, WHITE);
                    break;
                }

                case OBJ_MISSILE_HAMMER: {
                    draw_object(oid, TEX_MISSILE_HAMMER, FtFloat(object->values[VAL_MISSILE_ANGLE]), WHITE);
                    break;
                }

                case OBJ_EXPLODE: {
                    enum TextureIndices tex;
                    switch (object->values[VAL_EXPLODE_FRAME] / 100) {
                        default:
                            tex = TEX_EXPLODE1;
                            break;
                        case 1:
                            tex = TEX_EXPLODE2;
                            break;
                        case 2:
                            tex = TEX_EXPLODE3;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_ITEM_BLOCK: {
                    enum TextureIndices tex;
                    if (object->flags & FLG_BLOCK_EMPTY)
                        tex = TEX_EMPTY_BLOCK;
                    else
                        switch ((int)((float)(state.time) / 11.11111111111111f) % 4) {
                            default:
                                tex = TEX_ITEM_BLOCK1;
                                break;
                            case 1:
                            case 3:
                                tex = TEX_ITEM_BLOCK2;
                                break;
                            case 2:
                                tex = TEX_ITEM_BLOCK3;
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
                        (object->flags & FLG_BLOCK_GRAY) ? TEX_BRICK_BLOCK_GRAY : TEX_BRICK_BLOCK,
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
                            ? TEX_EMPTY_BLOCK
                            : ((object->flags & FLG_BLOCK_GRAY) ? TEX_BRICK_BLOCK_GRAY : TEX_BRICK_BLOCK),
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
                        oid, (object->flags & FLG_BLOCK_GRAY) ? TEX_BRICK_SHARD_GRAY : TEX_BRICK_SHARD,
                        FtFloat(object->values[VAL_BRICK_SHARD_ANGLE]), WHITE
                    );
                    break;
                }

                case OBJ_CHECKPOINT: {
                    draw_object(
                        oid,
                        (state.checkpoint == oid) ? (((state.time / 10) % 2) ? TEX_CHECKPOINT3 : TEX_CHECKPOINT2)
                                                  : TEX_CHECKPOINT1,
                        0, ALPHA(oid >= state.checkpoint ? 255 : 128)
                    );
                    break;
                }

                case OBJ_ROTODISC_BALL: {
                    draw_object(oid, TEX_ROTODISC_BALL, 0, WHITE);
                    break;
                }

                case OBJ_ROTODISC: {
                    enum TextureIndices tex;
                    switch (state.time % 26) {
                        default:
                            tex = TEX_ROTODISC1;
                            break;
                        case 1:
                            tex = TEX_ROTODISC2;
                            break;
                        case 2:
                            tex = TEX_ROTODISC3;
                            break;
                        case 3:
                            tex = TEX_ROTODISC4;
                            break;
                        case 4:
                            tex = TEX_ROTODISC5;
                            break;
                        case 5:
                            tex = TEX_ROTODISC6;
                            break;
                        case 6:
                            tex = TEX_ROTODISC7;
                            break;
                        case 7:
                            tex = TEX_ROTODISC8;
                            break;
                        case 8:
                            tex = TEX_ROTODISC9;
                            break;
                        case 9:
                            tex = TEX_ROTODISC10;
                            break;
                        case 10:
                            tex = TEX_ROTODISC11;
                            break;
                        case 11:
                            tex = TEX_ROTODISC12;
                            break;
                        case 12:
                            tex = TEX_ROTODISC13;
                            break;
                        case 13:
                            tex = TEX_ROTODISC14;
                            break;
                        case 14:
                            tex = TEX_ROTODISC15;
                            break;
                        case 15:
                            tex = TEX_ROTODISC16;
                            break;
                        case 16:
                            tex = TEX_ROTODISC17;
                            break;
                        case 17:
                            tex = TEX_ROTODISC18;
                            break;
                        case 18:
                            tex = TEX_ROTODISC19;
                            break;
                        case 19:
                            tex = TEX_ROTODISC20;
                            break;
                        case 20:
                            tex = TEX_ROTODISC21;
                            break;
                        case 21:
                            tex = TEX_ROTODISC22;
                            break;
                        case 22:
                            tex = TEX_ROTODISC23;
                            break;
                        case 23:
                            tex = TEX_ROTODISC24;
                            break;
                        case 24:
                            tex = TEX_ROTODISC25;
                            break;
                        case 25:
                            tex = TEX_ROTODISC26;
                            break;
                    }

                    set_batch_logic(GL_OR_REVERSE);
                    draw_object(oid, tex, 0, WHITE);
                    set_batch_logic(GL_COPY);
                    break;
                }

                case OBJ_WATER_SPLASH: {
                    enum TextureIndices tex;
                    switch (object->values[VAL_WATER_SPLASH_FRAME] / 10) {
                        default:
                            tex = TEX_WATER_SPLASH1;
                            break;
                        case 1:
                            tex = TEX_WATER_SPLASH2;
                            break;
                        case 2:
                            tex = TEX_WATER_SPLASH3;
                            break;
                        case 3:
                            tex = TEX_WATER_SPLASH4;
                            break;
                        case 4:
                            tex = TEX_WATER_SPLASH5;
                            break;
                        case 5:
                            tex = TEX_WATER_SPLASH6;
                            break;
                        case 6:
                            tex = TEX_WATER_SPLASH7;
                            break;
                        case 7:
                            tex = TEX_WATER_SPLASH8;
                            break;
                        case 8:
                            tex = TEX_WATER_SPLASH9;
                            break;
                        case 9:
                            tex = TEX_WATER_SPLASH10;
                            break;
                        case 10:
                            tex = TEX_WATER_SPLASH11;
                            break;
                        case 11:
                            tex = TEX_WATER_SPLASH12;
                            break;
                        case 12:
                            tex = TEX_WATER_SPLASH13;
                            break;
                        case 13:
                            tex = TEX_WATER_SPLASH14;
                            break;
                        case 14:
                            tex = TEX_WATER_SPLASH15;
                            break;
                    }
                    draw_object(oid, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUBBLE: {
                    enum TextureIndices tex;
                    float pos[3] = {FtInt(object->pos[0]), FtInt(object->pos[1]), FtFloat(object->depth)};
                    if (object->flags & FLG_BUBBLE_POP) {
                        switch (object->values[VAL_BUBBLE_FRAME]) {
                            default:
                                tex = TEX_BUBBLE_POP1;
                                break;
                            case 1:
                                tex = TEX_BUBBLE_POP2;
                                break;
                            case 2:
                                tex = TEX_BUBBLE_POP3;
                                break;
                            case 3:
                                tex = TEX_BUBBLE_POP4;
                                break;
                            case 4:
                                tex = TEX_BUBBLE_POP5;
                                break;
                            case 5:
                                tex = TEX_BUBBLE_POP6;
                                break;
                            case 6:
                                tex = TEX_BUBBLE_POP7;
                                break;
                        }
                    } else {
                        tex = TEX_BUBBLE;
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
                    draw_object(oid, TEX_GOAL_BAR1, 0, WHITE);
                    break;
                }

                case OBJ_GOAL_BAR_FLY: {
                    draw_object(oid, TEX_GOAL_BAR2, FtFloat(object->values[VAL_GOAL_ANGLE]), WHITE);
                    break;
                }

                case OBJ_PSWITCH: {
                    enum TextureIndices tex;
                    if (object->values[VAL_PSWITCH] > 0L)
                        tex = TEX_PSWITCH_FLAT;
                    else
                        switch ((int)((float)state.time / 3.703703703703704f) % 3) {
                            default:
                                tex = TEX_PSWITCH1;
                                break;
                            case 1:
                                tex = TEX_PSWITCH2;
                                break;
                            case 2:
                                tex = TEX_PSWITCH3;
                                break;
                        }

                    draw_object(oid, tex, 0, WHITE);
                    break;
                }
            }

        oid = object->previous;
    }

    enum TextureIndices tex;
    switch ((state.time / 5) % 8) {
        default:
            tex = TEX_WATER1;
            break;
        case 1:
            tex = TEX_WATER2;
            break;
        case 2:
            tex = TEX_WATER3;
            break;
        case 3:
            tex = TEX_WATER4;
            break;
        case 4:
            tex = TEX_WATER5;
            break;
        case 5:
            tex = TEX_WATER6;
            break;
        case 6:
            tex = TEX_WATER7;
            break;
        case 7:
            tex = TEX_WATER8;
            break;
    }
    const float x1 = 0;
    const float y1 = FtInt(state.water);
    const float x2 = FtInt(state.size[0]);
    const float y2 = FtInt(state.size[1]);
    draw_rectangle(tex, (float[2][2]){{x1, y1}, {x2, y1 + 16}}, -100, ALPHA(135));
    draw_rectangle(TEX_NULL, (float[2][2]){{x1, y1 + 16}, {x2, y2}}, -100, RGBA(88, 136, 224, 135));
}

void draw_state_hud() {
    const struct GamePlayer* player = get_player(local_player);
    if (player != NULL) {
        static char str[16];
        SDL_snprintf(str, sizeof(str), "MARIO * %u", player->lives);
        draw_text(FNT_HUD, FA_LEFT, str, (float[3]){32, 16, 0});

        SDL_snprintf(str, sizeof(str), "%u", player->score);
        draw_text(FNT_HUD, FA_RIGHT, str, (float[3]){149, 34, 0});

        enum TextureIndices tex;
        switch ((int)((float)(state.time) / 6.25f) % 4) {
            default:
                tex = TEX_HUD_COINS1;
                break;
            case 1:
            case 3:
                tex = TEX_HUD_COINS2;
                break;
            case 2:
                tex = TEX_HUD_COINS3;
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

void load_object(enum GameObjectType type) {
    switch (type) {
        default:
            break;

        case OBJ_CLOUD: {
            load_texture(TEX_CLOUD1);
            load_texture(TEX_CLOUD2);
            load_texture(TEX_CLOUD3);
            break;
        }

        case OBJ_BUSH: {
            load_texture(TEX_BUSH1);
            load_texture(TEX_BUSH2);
            load_texture(TEX_BUSH3);
            break;
        }

        case OBJ_BUSH_SNOW: {
            load_texture(TEX_BUSH_SNOW1);
            load_texture(TEX_BUSH_SNOW2);
            load_texture(TEX_BUSH_SNOW3);
            break;
        }

        case OBJ_PLAYER_SPAWN: {
            load_object(OBJ_PLAYER);
            break;
        }

        case OBJ_PLAYER: {
            load_texture(TEX_MARIO_SMALL);
            load_texture(TEX_MARIO_SMALL_WALK1);
            load_texture(TEX_MARIO_SMALL_WALK2);
            load_texture(TEX_MARIO_SMALL_JUMP);
            load_texture(TEX_MARIO_SMALL_SWIM1);
            load_texture(TEX_MARIO_SMALL_SWIM2);
            load_texture(TEX_MARIO_SMALL_SWIM3);
            load_texture(TEX_MARIO_SMALL_SWIM4);

            load_texture(TEX_MARIO_BIG_GROW);
            load_texture(TEX_MARIO_BIG);
            load_texture(TEX_MARIO_BIG_WALK1);
            load_texture(TEX_MARIO_BIG_WALK2);
            load_texture(TEX_MARIO_BIG_JUMP);
            load_texture(TEX_MARIO_BIG_DUCK);
            load_texture(TEX_MARIO_BIG_SWIM1);
            load_texture(TEX_MARIO_BIG_SWIM2);
            load_texture(TEX_MARIO_BIG_SWIM3);
            load_texture(TEX_MARIO_BIG_SWIM4);

            load_texture(TEX_MARIO_FIRE_GROW1);
            load_texture(TEX_MARIO_FIRE_GROW2);
            load_texture(TEX_MARIO_FIRE);
            load_texture(TEX_MARIO_FIRE_WALK1);
            load_texture(TEX_MARIO_FIRE_WALK2);
            load_texture(TEX_MARIO_FIRE_JUMP);
            load_texture(TEX_MARIO_FIRE_DUCK);
            load_texture(TEX_MARIO_FIRE_FIRE);
            load_texture(TEX_MARIO_FIRE_SWIM1);
            load_texture(TEX_MARIO_FIRE_SWIM2);
            load_texture(TEX_MARIO_FIRE_SWIM3);
            load_texture(TEX_MARIO_FIRE_SWIM4);

            load_texture(TEX_MARIO_BEETROOT);
            load_texture(TEX_MARIO_BEETROOT_WALK1);
            load_texture(TEX_MARIO_BEETROOT_WALK2);
            load_texture(TEX_MARIO_BEETROOT_JUMP);
            load_texture(TEX_MARIO_BEETROOT_DUCK);
            load_texture(TEX_MARIO_BEETROOT_FIRE);
            load_texture(TEX_MARIO_BEETROOT_SWIM1);
            load_texture(TEX_MARIO_BEETROOT_SWIM2);
            load_texture(TEX_MARIO_BEETROOT_SWIM3);
            load_texture(TEX_MARIO_BEETROOT_SWIM4);

            load_texture(TEX_MARIO_LUI);
            load_texture(TEX_MARIO_LUI_WALK1);
            load_texture(TEX_MARIO_LUI_WALK2);
            load_texture(TEX_MARIO_LUI_JUMP);
            load_texture(TEX_MARIO_LUI_DUCK);
            load_texture(TEX_MARIO_LUI_SWIM1);
            load_texture(TEX_MARIO_LUI_SWIM2);
            load_texture(TEX_MARIO_LUI_SWIM3);
            load_texture(TEX_MARIO_LUI_SWIM4);

            load_texture(TEX_MARIO_HAMMER);
            load_texture(TEX_MARIO_HAMMER_WALK1);
            load_texture(TEX_MARIO_HAMMER_WALK2);
            load_texture(TEX_MARIO_HAMMER_JUMP);
            load_texture(TEX_MARIO_HAMMER_DUCK);
            load_texture(TEX_MARIO_HAMMER_FIRE);
            load_texture(TEX_MARIO_HAMMER_SWIM1);
            load_texture(TEX_MARIO_HAMMER_SWIM2);
            load_texture(TEX_MARIO_HAMMER_SWIM3);
            load_texture(TEX_MARIO_HAMMER_SWIM4);

            load_texture(TEX_HUD_COINS1);
            load_texture(TEX_HUD_COINS2);
            load_texture(TEX_HUD_COINS3);

            load_font(FNT_HUD);

            load_sound(SND_JUMP);
            load_sound(SND_FIRE);
            load_sound(SND_GROW);
            load_sound(SND_WARP);
            load_sound(SND_BUMP);
            load_sound(SND_STOMP);
            load_sound(SND_STARMAN);
            load_sound(SND_SWIM);
            load_sound(SND_RESPAWN);

            load_track(MUS_STARMAN);

            load_object(OBJ_PLAYER_EFFECT);
            load_object(OBJ_PLAYER_DEAD);
            load_object(OBJ_MISSILE_FIREBALL);
            load_object(OBJ_MISSILE_BEETROOT);
            load_object(OBJ_MISSILE_HAMMER);
            load_object(OBJ_BUBBLE);
            break;
        }

        case OBJ_PLAYER_DEAD: {
            load_texture(TEX_MARIO_DEAD);

            load_sound(SND_LOSE);
            load_sound(SND_DEAD);
            load_sound(SND_HARDCORE);

            load_track(MUS_LOSE1);
            load_track(MUS_LOSE2);
            load_track(MUS_GAME_OVER);
            load_track(MUS_WIN1);
            load_track(MUS_WIN2);
            break;
        }

        case OBJ_COIN:
        case OBJ_PSWITCH_COIN: {
            load_texture(TEX_COIN1);
            load_texture(TEX_COIN2);
            load_texture(TEX_COIN3);

            load_sound(SND_COIN);

            load_object(OBJ_POINTS);
            load_object(OBJ_COIN_POP);
            break;
        }

        case OBJ_COIN_POP: {
            load_texture(TEX_COIN_POP1);
            load_texture(TEX_COIN_POP2);
            load_texture(TEX_COIN_POP3);
            load_texture(TEX_COIN_POP4);
            load_texture(TEX_COIN_SPARK1);
            load_texture(TEX_COIN_SPARK2);
            load_texture(TEX_COIN_SPARK3);
            load_texture(TEX_COIN_SPARK4);

            load_sound(SND_COIN);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM: {
            load_texture(TEX_MUSHROOM);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM_1UP: {
            load_texture(TEX_MUSHROOM_1UP);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MUSHROOM_POISON: {
            load_texture(TEX_MUSHROOM_POISON1);
            load_texture(TEX_MUSHROOM_POISON2);

            load_object(OBJ_EXPLODE);
            break;
        }

        case OBJ_FIRE_FLOWER: {
            load_texture(TEX_FIRE_FLOWER1);
            load_texture(TEX_FIRE_FLOWER2);
            load_texture(TEX_FIRE_FLOWER3);
            load_texture(TEX_FIRE_FLOWER4);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_BEETROOT: {
            load_texture(TEX_BEETROOT1);
            load_texture(TEX_BEETROOT2);
            load_texture(TEX_BEETROOT3);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_LUI: {
            load_texture(TEX_LUI1);
            load_texture(TEX_LUI2);
            load_texture(TEX_LUI3);
            load_texture(TEX_LUI4);
            load_texture(TEX_LUI5);
            load_texture(TEX_LUI_BOUNCE1);
            load_texture(TEX_LUI_BOUNCE2);
            load_texture(TEX_LUI_BOUNCE3);

            load_sound(SND_KICK);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_HAMMER_SUIT: {
            load_texture(TEX_HAMMER_SUIT);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_STARMAN: {
            load_texture(TEX_STARMAN1);
            load_texture(TEX_STARMAN2);
            load_texture(TEX_STARMAN3);
            load_texture(TEX_STARMAN4);
            break;
        }

        case OBJ_POINTS: {
            load_texture(TEX_100);
            load_texture(TEX_200);
            load_texture(TEX_500);
            load_texture(TEX_1000);
            load_texture(TEX_2000);
            load_texture(TEX_5000);
            load_texture(TEX_10000);
            load_texture(TEX_1000000);
            load_texture(TEX_1UP);

            load_sound(SND_1UP);
            break;
        }

        case OBJ_MISSILE_FIREBALL: {
            load_texture(TEX_MISSILE_FIREBALL);

            load_sound(SND_BUMP);
            load_sound(SND_KICK);

            load_object(OBJ_EXPLODE);
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_MISSILE_BEETROOT: {
            load_texture(TEX_MISSILE_BEETROOT);

            load_sound(SND_HURT);
            load_sound(SND_BUMP);
            load_sound(SND_KICK);

            load_object(OBJ_EXPLODE);
            load_object(OBJ_POINTS);
            load_object(OBJ_MISSILE_BEETROOT_SINK);
            break;
        }

        case OBJ_MISSILE_BEETROOT_SINK: {
            load_texture(TEX_MISSILE_BEETROOT);

            load_object(OBJ_BUBBLE);
            break;
        }

        case OBJ_MISSILE_HAMMER: {
            load_texture(TEX_MISSILE_HAMMER);

            load_sound(SND_KICK);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_EXPLODE: {
            load_texture(TEX_EXPLODE1);
            load_texture(TEX_EXPLODE2);
            load_texture(TEX_EXPLODE3);
            break;
        }

        case OBJ_ITEM_BLOCK: {
            load_texture(TEX_ITEM_BLOCK1);
            load_texture(TEX_ITEM_BLOCK2);
            load_texture(TEX_ITEM_BLOCK3);
            load_texture(TEX_EMPTY_BLOCK);

            load_sound(SND_SPROUT);

            load_object(OBJ_BLOCK_BUMP);
            break;
        }

        case OBJ_BRICK_BLOCK:
        case OBJ_PSWITCH_BRICK: {
            load_texture(TEX_BRICK_BLOCK);

            load_sound(SND_BUMP);
            load_sound(SND_BREAK);

            load_object(OBJ_BRICK_SHARD);
            break;
        }

        case OBJ_BRICK_SHARD: {
            load_texture(TEX_BRICK_SHARD);
            break;
        }

        case OBJ_BRICK_BLOCK_COIN: {
            load_texture(TEX_BRICK_BLOCK);
            load_texture(TEX_EMPTY_BLOCK);

            load_object(OBJ_COIN_POP);
            break;
        }

        case OBJ_CHECKPOINT: {
            load_texture(TEX_CHECKPOINT1);
            load_texture(TEX_CHECKPOINT2);
            load_texture(TEX_CHECKPOINT3);

            load_sound(SND_SPROUT);

            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_ROTODISC_BALL: {
            load_texture(TEX_ROTODISC_BALL);
            load_object(OBJ_ROTODISC);
            break;
        }

        case OBJ_ROTODISC: {
            load_texture(TEX_ROTODISC1);
            load_texture(TEX_ROTODISC2);
            load_texture(TEX_ROTODISC3);
            load_texture(TEX_ROTODISC4);
            load_texture(TEX_ROTODISC5);
            load_texture(TEX_ROTODISC6);
            load_texture(TEX_ROTODISC7);
            load_texture(TEX_ROTODISC8);
            load_texture(TEX_ROTODISC9);
            load_texture(TEX_ROTODISC10);
            load_texture(TEX_ROTODISC11);
            load_texture(TEX_ROTODISC12);
            load_texture(TEX_ROTODISC13);
            load_texture(TEX_ROTODISC14);
            load_texture(TEX_ROTODISC15);
            load_texture(TEX_ROTODISC16);
            load_texture(TEX_ROTODISC17);
            load_texture(TEX_ROTODISC18);
            load_texture(TEX_ROTODISC19);
            load_texture(TEX_ROTODISC20);
            load_texture(TEX_ROTODISC21);
            load_texture(TEX_ROTODISC22);
            load_texture(TEX_ROTODISC23);
            load_texture(TEX_ROTODISC24);
            load_texture(TEX_ROTODISC25);
            load_texture(TEX_ROTODISC26);
            break;
        }

        case OBJ_GOAL_MARK: {
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_WATER_SPLASH: {
            load_texture(TEX_WATER_SPLASH1);
            load_texture(TEX_WATER_SPLASH2);
            load_texture(TEX_WATER_SPLASH3);
            load_texture(TEX_WATER_SPLASH4);
            load_texture(TEX_WATER_SPLASH5);
            load_texture(TEX_WATER_SPLASH6);
            load_texture(TEX_WATER_SPLASH7);
            load_texture(TEX_WATER_SPLASH8);
            load_texture(TEX_WATER_SPLASH9);
            load_texture(TEX_WATER_SPLASH10);
            load_texture(TEX_WATER_SPLASH11);
            load_texture(TEX_WATER_SPLASH12);
            load_texture(TEX_WATER_SPLASH13);
            load_texture(TEX_WATER_SPLASH14);
            load_texture(TEX_WATER_SPLASH15);
            break;
        }

        case OBJ_BUBBLE: {
            load_texture(TEX_BUBBLE);
            load_texture(TEX_BUBBLE_POP1);
            load_texture(TEX_BUBBLE_POP2);
            load_texture(TEX_BUBBLE_POP3);
            load_texture(TEX_BUBBLE_POP4);
            load_texture(TEX_BUBBLE_POP5);
            load_texture(TEX_BUBBLE_POP6);
            load_texture(TEX_BUBBLE_POP7);
            break;
        }

        case OBJ_GOAL_BAR: {
            load_texture(TEX_GOAL_BAR1);

            load_object(OBJ_GOAL_BAR_FLY);
            load_object(OBJ_POINTS);
            break;
        }

        case OBJ_GOAL_BAR_FLY: {
            load_texture(TEX_GOAL_BAR2);
            break;
        }

        case OBJ_PSWITCH: {
            load_texture(TEX_PSWITCH1);
            load_texture(TEX_PSWITCH2);
            load_texture(TEX_PSWITCH3);
            load_texture(TEX_PSWITCH_FLAT);

            load_sound(SND_TOGGLE);
            load_sound(SND_STARMAN);

            load_track(MUS_PSWITCH);

            load_object(OBJ_PSWITCH_COIN);
            load_object(OBJ_PSWITCH_BRICK);
            break;
        }
    }
}

bool object_is_alive(ObjectID index) {
    return index >= 0L && index < MAX_OBJECTS && state.objects[index].type != OBJ_NULL;
}

struct GameObject* get_object(ObjectID index) {
    return object_is_alive(index) ? &(state.objects[index]) : NULL;
}

ObjectID create_object(enum GameObjectType type, const fvec2 pos) {
    if (type == OBJ_NULL)
        return -1L;

    ObjectID index = state.next_object;
    for (size_t i = 0; i < MAX_OBJECTS; i++) {
        if (!object_is_alive((ObjectID)index)) {
            load_object(type);
            struct GameObject* object = &state.objects[index];
            SDL_memset(object, 0, sizeof(struct GameObject));

            object->type = type;
            object->flags = FLG_VISIBLE;
            object->previous = state.live_objects;
            object->next = -1L;
            if (object_is_alive(state.live_objects))
                state.objects[state.live_objects].next = index;
            state.live_objects = index;

            object->block = -1L;
            object->previous_block = object->next_block = -1L;
            move_object(index, pos);

            interp[index].type = type;
            interp[index].from[0] = interp[index].to[0] = interp[index].pos[0] = pos[0];
            interp[index].from[1] = interp[index].to[1] = interp[index].pos[1] = pos[1];

            switch (type) {
                default:
                    break;

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
                        object->values[i] = -1L;
                    for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++)
                        object->values[i] = -1L;
                    break;
                }

                case OBJ_PLAYER_DEAD: {
                    object->depth = -FxOne;

                    object->values[VAL_PLAYER_INDEX] = -1L;
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
                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(15L);

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_MUSHROOM_POISON: {
                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(15L);

                    object->values[VAL_X_SPEED] = FfInt(2L);
                    break;
                }

                case OBJ_FIRE_FLOWER: {
                    object->bbox[0][0] = FfInt(-17L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(16L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_BEETROOT: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_LUI: {
                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-30L);
                    object->bbox[1][0] = FfInt(15L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags |= FLG_POWERUP_FULL;
                    break;
                }

                case OBJ_STARMAN: {
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

                    object->values[VAL_MISSILE_OWNER] = -1L;
                    break;
                }

                case OBJ_MISSILE_HAMMER: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-18L);
                    object->bbox[1][0] = FfInt(11L);
                    object->bbox[1][1] = FfInt(15L);

                    object->values[VAL_MISSILE_OWNER] = -1L;
                    break;
                }

                case OBJ_MISSILE_BEETROOT:
                case OBJ_MISSILE_BEETROOT_SINK: {
                    object->bbox[0][0] = FfInt(-11L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(12L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_MISSILE_OWNER] = -1L;
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

                    object->values[VAL_BLOCK_BUMP_OWNER] = -1L;
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

                    object->values[VAL_ROTODISC_OWNER] = -1L;
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
                    break;
                }
            }

            state.next_object = (ObjectID)((index + 1L) % MAX_OBJECTS);
            return index;
        }
        index = (ObjectID)((index + 1L) % MAX_OBJECTS);
    }
    return -1L;
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
    object->next_block = -1L;
    if (object_is_alive(state.blockmap[block]))
        state.objects[state.blockmap[block]].next_block = index;
    state.blockmap[block] = index;
}

void list_block_at(struct BlockList* list, const fvec2 rect[2]) {
    list->num_objects = 0;

    int bx1 = Fsub(rect[0][0], BLOCK_SIZE) / BLOCK_SIZE;
    int by1 = Fsub(rect[0][1], BLOCK_SIZE) / BLOCK_SIZE;
    int bx2 = Fadd(rect[1][0], BLOCK_SIZE) / BLOCK_SIZE;
    int by2 = Fadd(rect[1][1], BLOCK_SIZE) / BLOCK_SIZE;
    bx1 = SDL_clamp(bx1, 0L, MAX_BLOCKS - 1L);
    by1 = SDL_clamp(by1, 0L, MAX_BLOCKS - 1L);
    bx2 = SDL_clamp(bx2, 0L, MAX_BLOCKS - 1L);
    by2 = SDL_clamp(by2, 0L, MAX_BLOCKS - 1L);
    for (int by = by1; by <= by2; by++)
        for (int bx = bx1; bx <= bx2; bx++) {
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
                state.spawn = -1L;
            break;
        }

        case OBJ_PLAYER: {
            struct GamePlayer* player = get_player(object->values[VAL_PLAYER_INDEX]);
            if (player != NULL && player->object == index)
                player->object = -1L;

            for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++) {
                struct GameObject* missile = get_object((ObjectID)(object->values[i]));
                if (missile != NULL)
                    missile->values[VAL_MISSILE_OWNER] = -1L;
            }
            for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++) {
                struct GameObject* beetroot = get_object((ObjectID)(object->values[i]));
                if (beetroot != NULL)
                    beetroot->values[VAL_MISSILE_OWNER] = -1L;
            }

            break;
        }

        case OBJ_MISSILE_FIREBALL:
        case OBJ_MISSILE_BEETROOT:
        case OBJ_MISSILE_HAMMER: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_MISSILE_OWNER]));
            if (owner != NULL && owner->type == OBJ_PLAYER)
                for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                    if (owner->values[i] == index) {
                        owner->values[i] = -1L;
                        break;
                    }
            break;
        }

        case OBJ_MISSILE_BEETROOT_SINK: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_MISSILE_OWNER]));
            if (owner != NULL && owner->type == OBJ_PLAYER)
                for (size_t i = VAL_PLAYER_BEETROOT_START; i < VAL_PLAYER_BEETROOT_END; i++)
                    if (owner->values[i] == index) {
                        owner->values[i] = -1L;
                        break;
                    }
            break;
        }

        case OBJ_CHECKPOINT: {
            if (state.checkpoint == index)
                state.checkpoint = -1L;
            break;
        }

        case OBJ_ROTODISC_BALL: {
            kill_object((ObjectID)(object->values[VAL_ROTODISC]));
            break;
        }

        case OBJ_ROTODISC: {
            struct GameObject* owner = get_object((ObjectID)(object->values[VAL_ROTODISC_OWNER]));
            if (owner != NULL && owner->type == OBJ_ROTODISC_BALL)
                owner->values[VAL_ROTODISC] = -1L;
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

void draw_object(ObjectID oid, enum TextureIndices tid, GLfloat angle, const GLubyte color[4]) {
    struct GameObject* object = &(state.objects[oid]);
    struct InterpObject* smooth = &(interp[oid]);
    draw_sprite(
        tid,
        (float[3]){FtInt(smooth->pos[0]), FtInt(smooth->pos[1]) + object->values[VAL_SPROUT],
                   (object->values[VAL_SPROUT] > 0L) ? 21 : FtFloat(object->depth)},
        (bool[2]){object->flags & FLG_X_FLIP, object->flags & FLG_Y_FLIP}, angle, color
    );
}

void play_sound_at_object(struct GameObject* object, enum SoundIndices sid) {
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

    const struct GamePlayer* player = get_player(local_player);
    if (player != NULL) {
        const ObjectID pwid = player->object;
        if (object_is_alive(pwid)) {
            const float cx = FtInt(interp[pwid].pos[0]);
            const float cy = FtInt(interp[pwid].pos[1]);
            const float bx1 = FtFloat(player->bounds[0][0]) + HALF_SCREEN_WIDTH;
            const float by1 = FtFloat(player->bounds[0][1]) + HALF_SCREEN_HEIGHT;
            const float bx2 = FtFloat(player->bounds[1][0]) - HALF_SCREEN_WIDTH;
            const float by2 = FtFloat(player->bounds[1][1]) - HALF_SCREEN_HEIGHT;
            move_camera(SDL_clamp(cx, bx1, bx2), SDL_clamp(cy, by1, by2));
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
