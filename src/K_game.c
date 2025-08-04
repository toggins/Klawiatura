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
}

static uint32_t fletcher32(uint8_t* data, uint32_t len) {
    uint32_t checksum = 0;
    while (len > 0)
        checksum += data[--len];
    return checksum;
}

void save_state(GekkoGameEvent* event) {
    *(event->data.save.state_len) = sizeof(state);
    *(event->data.save.checksum) = fletcher32((uint8_t*)(&state), sizeof(state));
    SDL_memcpy(event->data.save.state, &state, sizeof(state));
}

void load_state(GekkoGameEvent* event) {
    SDL_memcpy(&state, event->data.load.state, sizeof(state));
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
                if ((object->pos[1] <= FxZero && object->values[VAL_PLAYER_Y_SPEED] < FxZero) ||
                    object->pos[1] >= FfInt(480L))
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
                    if (player->input.action.fire && !(player->last_input.action.fire))
                        object->object_flags |= OBF_DESTROY;
                }

                move_object(
                    oid, (fvec2){Fclamp(Fadd(object->pos[0], object->values[VAL_PLAYER_X_SPEED]), FxZero, FfInt(640L)),
                                 Fclamp(Fadd(object->pos[1], object->values[VAL_PLAYER_Y_SPEED]), FxZero, FfInt(480L))}
                );
                break;
            }
        }

        ObjectID next = object->previous;
        if (object->object_flags & OBF_DESTROY)
            destroy_object(oid);
        oid = next;
    }
}

void draw_state(SDL_Renderer* renderer) {
    float cx = 0, cy = 0;
    if (local_player >= 0 && local_player < MAX_PLAYERS) {
        struct GamePlayer* player = &(state.players[local_player]);
        if (object_is_alive(player->object)) {
            struct GameObject* object = &(state.objects[player->object]);
            cx = FtFloat(object->pos[0]) - 320;
            cy = FtFloat(object->pos[1]) - 240;
        }
    }

    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        struct GameObject* object = &(state.objects[oid]);
        if (object->object_flags & OBF_VISIBLE)
            switch (object->type) {
                default:
                    break;

                case OBJ_PLAYER: {
                    switch (object->values[VAL_PLAYER_INDEX]) {
                        default:
                            break;
                        case 0:
                            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                            break;
                        case 1:
                            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                            break;
                        case 2:
                            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                            break;
                        case 3:
                            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
                            break;
                    }
                    SDL_RenderFillRect(
                        renderer,
                        &(SDL_FRect){FtFloat(object->pos[0]) - 16 - cx, FtFloat(object->pos[1]) - 16 - cy, 32, 32}
                    );

                    break;
                }
            }

        oid = object->previous;
    }
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
            if (object_is_alive(state.live_objects))
                state.objects[state.live_objects].next = index;
            object->previous = state.live_objects;
            object->next = -1L;
            state.live_objects = index;

            object->block = -1L;
            object->previous_block = object->next_block = -1L;
            move_object(index, pos);

            switch (type) {
                default:
                    break;

                case OBJ_PLAYER: {
                    object->values[VAL_PLAYER_INDEX] = -FxOne;
                    object->values[VAL_PLAYER_X_SPEED] = FxZero;
                    object->values[VAL_PLAYER_Y_SPEED] = FxZero;
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

    int32_t block = object->block;
    if (block > 0L && block < (int32_t)BLOCKMAP_SIZE) {
        if (state.blockmap[block] == index)
            state.blockmap[block] = object->previous_block;
        if (object_is_alive(object->previous_block))
            state.objects[object->previous_block].next_block = object->next_block;
        if (object_is_alive(object->next_block))
            state.objects[object->next_block].previous_block = object->previous_block;
    }

    int32_t bx = FtInt(Fdiv(object->pos[0], BLOCK_SIZE));
    int32_t by = FtInt(Fdiv(object->pos[1], BLOCK_SIZE));
    bx = SDL_clamp(bx, 0L, MAX_BLOCKS - 1L);
    by = SDL_clamp(by, 0L, MAX_BLOCKS - 1L);
    block = bx + (by * MAX_BLOCKS);

    if (object_is_alive(state.blockmap[block]))
        state.objects[state.blockmap[block]].next_block = index;
    object->block = block;
    object->previous_block = state.blockmap[block];
    object->next_block = -1L;
    state.blockmap[block] = index;
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

    if (state.live_objects == index)
        state.live_objects = object->previous;
    if (object_is_alive(object->previous))
        state.objects[object->previous].next = object->next;
    if (object_is_alive(object->next))
        state.objects[object->next].previous = object->previous;

    state.next_object = SDL_min(state.next_object, index);
}

bool object_is_alive(ObjectID index) {
    return index >= 0L && index < MAX_OBJECTS && state.objects[index].type != OBJ_INVALID;
}
