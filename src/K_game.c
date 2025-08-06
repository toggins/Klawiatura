#include "K_game.h"

static struct GameState state = {0};
static int local_player = -1;

void start_state(int num_players, int local) {
    local_player = local;
    state.live_objects = -1L;
    for (uint32_t i = 0L; i < BLOCKMAP_SIZE; i++)
        state.blockmap[i] = -1L;

    for (size_t i = 0; i < num_players; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->active = true;

        ObjectID object = create_object(OBJ_PLAYER, (fvec2){FfInt(320L), FfInt(240L)});
        if (object_is_alive(object))
            state.objects[object].values[VAL_PLAYER_INDEX] = player->object = object;
        player->object = object;
    }

    create_object(OBJ_BUSH, (fvec2){FfInt(32L), FfInt(448L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(128L), FfInt(32L)});
}

static uint32_t check_state(uint8_t* data, uint32_t len) {
    uint32_t checksum = 0;
    while (len > 0)
        checksum += data[--len];
    return checksum;
}

void save_state(GekkoGameEvent* event) {
    *(event->data.save.state_len) = sizeof(state);
    *(event->data.save.checksum) = check_state((uint8_t*)(&state), sizeof(state));
    SDL_memcpy(event->data.save.state, &state, sizeof(state));
}

void load_state(GekkoGameEvent* event) {
    SDL_memcpy(&state, event->data.load.state, sizeof(state));
}

static bool player_collide(ObjectID self, ObjectID other) {
    if (state.objects[other].type == OBJ_PLAYER) {
        state.objects[self].flags |= FLG_PLAYER_COLLISION;
        return false;
    }
    return true;
}

static bool bullet_collide(ObjectID self, ObjectID other) {
    struct GameObject* hit = &(state.objects[other]);
    if (hit->type != OBJ_BULLET &&
        (hit->type != OBJ_PLAYER || hit->values[VAL_PLAYER_INDEX] != state.objects[self].values[VAL_BULLET_PLAYER])) {
        state.objects[self].values[VAL_BULLET_FRAME] += 24L;
        return false;
    }
    return true;
}

void tick_state(struct GameInput inputs[MAX_PLAYERS]) {
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->last_input.value = player->input.value;
        player->input.value = inputs[i].value;
    }

    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        struct GameObject* object = &(state.objects[oid]);

        switch (object->type) {
            default:
                break;

            case OBJ_PLAYER: {
                object->values[VAL_PLAYER_X_SPEED] = Fmul(object->values[VAL_PLAYER_X_SPEED], 0x0000E666);
                object->values[VAL_PLAYER_Y_SPEED] = Fadd(object->values[VAL_PLAYER_Y_SPEED], 0x0000A666);
                if ((object->pos[0] <= -object->bbox[0][0] && object->values[VAL_PLAYER_X_SPEED] < FxZero) ||
                    (object->pos[0] >= Fsub(F_SCREEN_WIDTH, object->bbox[1][0]) &&
                     object->values[VAL_PLAYER_X_SPEED] > FxZero))
                    object->values[VAL_PLAYER_X_SPEED] = FxZero;
                if ((object->pos[1] <= -object->bbox[0][1] && object->values[VAL_PLAYER_Y_SPEED] < FxZero) ||
                    (object->pos[1] >= Fsub(F_SCREEN_HEIGHT, object->bbox[1][1]) &&
                     object->values[VAL_PLAYER_Y_SPEED] > FxZero))
                    object->values[VAL_PLAYER_Y_SPEED] = FxZero;

                fix16_t pid = object->values[VAL_PLAYER_INDEX];
                if (pid >= 0L && pid < MAX_PLAYERS) {
                    struct GamePlayer* player = &(state.players[pid]);
                    object->values[VAL_PLAYER_X_SPEED] = Fadd(
                        object->values[VAL_PLAYER_X_SPEED],
                        FfInt((int8_t)player->input.action.right - (int8_t)player->input.action.left)
                    );
                    object->values[VAL_PLAYER_Y_SPEED] = Fadd(
                        object->values[VAL_PLAYER_Y_SPEED],
                        FfInt((int8_t)player->input.action.down - (int8_t)player->input.action.up)
                    );

                    if (player->input.action.jump && !(player->last_input.action.jump))
                        object->values[VAL_PLAYER_Y_SPEED] = FfInt(-16L);

                    if (player->input.action.fire && !(player->last_input.action.fire) &&
                        (Fabs(object->values[VAL_PLAYER_X_SPEED]) >= FxHalf ||
                         Fabs(object->values[VAL_PLAYER_Y_SPEED]) >= FxHalf)) {
                        ObjectID bullet_id = create_object(OBJ_BULLET, object->pos);
                        if (object_is_alive(bullet_id)) {
                            struct GameObject* bullet = &(state.objects[bullet_id]);
                            bullet->values[VAL_BULLET_PLAYER] = object->values[VAL_PLAYER_INDEX];
                            bullet->values[VAL_BULLET_X_SPEED] = Fmul(object->values[VAL_PLAYER_X_SPEED], FfInt(2L));
                            bullet->values[VAL_BULLET_Y_SPEED] = Fmul(object->values[VAL_PLAYER_Y_SPEED], FfInt(2L));
                        }
                    }
                }

                if (object->values[VAL_PLAYER_X_SPEED] > FxZero)
                    object->object_flags &= ~OBF_X_FLIP;
                else if (object->values[VAL_PLAYER_X_SPEED] < FxZero)
                    object->object_flags |= OBF_X_FLIP;

                move_object(
                    oid, (fvec2){Fclamp(
                                     Fadd(object->pos[0], object->values[VAL_PLAYER_X_SPEED]), -object->bbox[0][0],
                                     Fsub(F_SCREEN_WIDTH, object->bbox[1][0])
                                 ),
                                 Fclamp(
                                     Fadd(object->pos[1], object->values[VAL_PLAYER_Y_SPEED]), -object->bbox[0][1],
                                     Fsub(F_SCREEN_HEIGHT, object->bbox[1][1])
                                 )}
                );
                object->flags &= ~FLG_PLAYER_COLLISION;
                iterate_block(oid, player_collide);
                break;
            }

            case OBJ_BULLET: {
                if (object->values[VAL_BULLET_FRAME] > 0L) {
                    object->values[VAL_BULLET_FRAME] += 24L;
                    if (object->values[VAL_BULLET_FRAME] >= 300L)
                        object->object_flags |= OBF_DESTROY;
                    break;
                }

                move_object(
                    oid, (fvec2){Fadd(object->pos[0], object->values[VAL_BULLET_X_SPEED]),
                                 Fadd(object->pos[1], object->values[VAL_BULLET_Y_SPEED])}
                );
                if (object->pos[0] < FxZero || object->pos[1] < FxZero || object->pos[0] > F_SCREEN_WIDTH ||
                    object->pos[1] > F_SCREEN_HEIGHT)
                    object->values[VAL_BULLET_FRAME] += 24L;
                else
                    iterate_block(oid, bullet_collide);
                break;
            }
        }

        ObjectID next = object->previous;
        if (object->object_flags & OBF_DESTROY)
            destroy_object(oid);
        oid = next;
    }

    ++(state.time);
}

void draw_state() {
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        struct GameObject* object = &(state.objects[oid]);
        if (object->object_flags & OBF_VISIBLE)
            switch (object->type) {
                default:
                    break;

                case OBJ_CLOUD: {
                    enum TextureIndices tex;
                    switch ((state.time / 13) % 3) {
                        default:
                            tex = TEX_CLOUD1;
                            break;
                        case 1:
                            tex = TEX_CLOUD2;
                            break;
                        case 2:
                            tex = TEX_CLOUD3;
                            break;
                    }
                    draw_object(object, tex);
                    break;
                }

                case OBJ_BUSH: {
                    enum TextureIndices tex;
                    switch ((state.time / 7) % 3) {
                        default:
                            tex = TEX_BUSH1;
                            break;
                        case 1:
                            tex = TEX_BUSH2;
                            break;
                        case 2:
                            tex = TEX_BUSH3;
                            break;
                    }
                    draw_object(object, tex);
                    break;
                }

                case OBJ_PLAYER: {
                    draw_object(object, TEX_PLAYER);
                    break;
                }

                case OBJ_BULLET: {
                    const fix16_t frame = object->values[VAL_BULLET_FRAME];
                    draw_object(
                        object, frame <= 0 ? TEX_BULLET
                                           : ((frame > 0 && frame < 100)
                                                  ? TEX_BULLET_HIT1
                                                  : ((frame >= 100 && frame < 200) ? TEX_BULLET_HIT2 : TEX_BULLET_HIT3))
                    );
                    break;
                }
            }

        oid = object->previous;
    }
}

bool object_is_alive(ObjectID index) {
    return index >= 0L && index < MAX_OBJECTS && state.objects[index].type != OBJ_INVALID;
}

ObjectID create_object(enum GameObjectType type, const fvec2 pos) {
    if (type == OBJ_INVALID)
        return -1L;

    ObjectID index = state.next_object;
    for (size_t i = 0; i < MAX_OBJECTS; i++) {
        if (!object_is_alive((ObjectID)index)) {
            struct GameObject* object = &state.objects[index];

            object->type = type;
            object->object_flags = OBF_DEFAULT;
            object->previous = state.live_objects;
            object->next = -1L;
            if (object_is_alive(state.live_objects))
                state.objects[state.live_objects].next = index;
            state.live_objects = index;

            object->depth = 0L;
            object->flags = 0L;

            object->block = -1L;
            object->previous_block = object->next_block = -1L;
            move_object(index, pos);

            switch (type) {
                default:
                    break;

                case OBJ_CLOUD: {
                    load_texture(TEX_CLOUD1);
                    load_texture(TEX_CLOUD2);
                    load_texture(TEX_CLOUD3);
                    object->depth = 25L;
                    break;
                }

                case OBJ_BUSH: {
                    load_texture(TEX_BUSH1);
                    load_texture(TEX_BUSH2);
                    load_texture(TEX_BUSH3);
                    object->depth = 25L;
                    break;
                }

                case OBJ_PLAYER: {
                    load_texture(TEX_PLAYER);

                    object->bbox[0][0] = FfInt(-16L);
                    object->bbox[0][1] = FfInt(-16L);
                    object->bbox[1][0] = FfInt(16L);
                    object->bbox[1][1] = FfInt(16L);
                    object->depth = -1L;

                    object->values[VAL_PLAYER_INDEX] = -1L;
                    object->values[VAL_PLAYER_X_SPEED] = FxZero;
                    object->values[VAL_PLAYER_Y_SPEED] = FxZero;

                    object->flags = FLG_PLAYER_DEFAULT;
                    break;
                }

                case OBJ_BULLET: {
                    load_texture(TEX_BULLET);
                    load_texture(TEX_BULLET_HIT1);
                    load_texture(TEX_BULLET_HIT2);
                    load_texture(TEX_BULLET_HIT3);

                    object->bbox[0][0] = FfInt(-5L);
                    object->bbox[0][1] = FfInt(-5L);
                    object->bbox[1][0] = FfInt(5L);
                    object->bbox[1][1] = FfInt(5L);

                    object->values[VAL_BULLET_PLAYER] = -1L;
                    object->values[VAL_BULLET_X_SPEED] = FxZero;
                    object->values[VAL_BULLET_Y_SPEED] = FxZero;
                    object->values[VAL_BULLET_FRAME] = 0L;
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
    if (!object_is_alive(index))
        return;

    struct GameObject* object = &(state.objects[index]);
    if (fvec2_equal(pos, object->pos))
        return;
    fvec2_copy(pos, object->pos);

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

void iterate_block(ObjectID index, bool (*iterator)(ObjectID, ObjectID)) {
    if (!object_is_alive(index))
        return;

    struct GameObject* object = &(state.objects[index]);
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
                struct GameObject* other = &(state.objects[oid]);
                ObjectID next = other->previous_block;
                if (index != oid) {
                    fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                    fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                    fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                    fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                    if (x1 <= ox2 && x2 >= ox1 && y1 <= oy2 && y2 >= oy1 && !iterator(index, oid))
                        return;
                }
                oid = next;
            }
        }
}

void destroy_object(ObjectID index) {
    if (!object_is_alive(index))
        return;

    struct GameObject* object = &(state.objects[index]);

    switch (object->type) {
        default:
            break;

        case OBJ_PLAYER: {
            fix16_t pid = object->values[VAL_PLAYER_INDEX];
            if (pid >= 0L && pid < MAX_PLAYERS && state.players[pid].object == index)
                state.players[pid].object = -1L;
            break;
        }
    }
    object->type = OBJ_INVALID;

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

    state.next_object = SDL_min(state.next_object, index);
}

void draw_object(struct GameObject* object, enum TextureIndices tid) {
    draw_sprite(
        tid, (float[3]){FtFloat(object->pos[0]), FtFloat(object->pos[1]), object->depth},
        (bool[2]){object->object_flags & OBF_X_FLIP, object->object_flags & OBF_Y_FLIP}
    );
}
