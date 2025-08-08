#include "K_game.h"
#include "K_audio.h"

static struct GameState state = {0};
static int local_player = -1;

/* ====

   GAME

   ==== */

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
                struct GameObject* other = &(state.objects[oid]);
                if (other->type == OBJ_SOLID) {
                    fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                    fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                    fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                    fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                    if (rect[0][0] < ox2 && rect[1][0] > ox1 && rect[0][1] < oy2 && rect[1][1] > oy1)
                        return true;
                }
                oid = other->previous_block;
            }
        }

    return false;
}

static void displace_object(ObjectID did, fix16_t climb, bool unstuck) {
    if (!object_is_alive(did))
        return;
    struct GameObject* displacee = &(state.objects[did]);

    displacee->values[VAL_X_TOUCH] = 0L;
    displacee->values[VAL_Y_TOUCH] = 0L;

    fix16_t x = displacee->pos[0];
    fix16_t y = displacee->pos[1];

    // Horizontal collision
    x = Fadd(x, displacee->values[VAL_X_SPEED]);
    const struct BlockList* list = list_block_at((fvec2[2]){
        {Fadd(x, displacee->bbox[0][0]), Fadd(y, displacee->bbox[0][1])},
        {Fadd(x, displacee->bbox[1][0]), Fadd(y, displacee->bbox[1][1])},
    });
    bool climbed = false;

    if (list->num_objects > 0) {
        bool stop = false;
        if (displacee->values[VAL_X_SPEED] < FxZero) {
            for (size_t i = 0; i < list->num_objects; i++) {
                struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
                    continue;

                if (displacee->values[VAL_Y_SPEED] >= FxZero &&
                    Fsub(Fadd(displacee->pos[1], displacee->bbox[1][1]), climb) <
                        Fadd(object->pos[1], object->bbox[0][1])) {
                    fix16_t step = Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]);
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
            for (size_t i = 0; i < list->num_objects; i++) {
                struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
                    continue;

                if (displacee->values[VAL_Y_SPEED] >= FxZero &&
                    Fsub(Fadd(displacee->pos[1], displacee->bbox[1][1]), climb) <
                        Fadd(object->pos[1], object->bbox[0][1])) {
                    fix16_t step = Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]);
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
    list = list_block_at((fvec2[2]){
        {Fadd(x, displacee->bbox[0][0]), Fadd(y, displacee->bbox[0][1])},
        {Fadd(x, displacee->bbox[1][0]), Fadd(y, displacee->bbox[1][1])},
    });

    if (list->num_objects > 0) {
        bool stop = false;
        if (displacee->values[VAL_Y_SPEED] < FxZero) {
            for (size_t i = 0; i < list->num_objects; i++) {
                struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
                    continue;

                y = Fmax(y, Fsub(Fadd(object->pos[1], object->bbox[1][1]), displacee->bbox[0][1]));
                stop = true;
            }
            displacee->values[VAL_Y_TOUCH] = -stop;
        } else if (displacee->values[VAL_Y_SPEED] > FxZero) {
            for (size_t i = 0; i < list->num_objects; i++) {
                struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID && (object->type != OBJ_SOLID_TOP ||
                                                  Fsub(Fadd(y, displacee->bbox[1][1]), displacee->values[VAL_Y_SPEED]) >
                                                      Fadd(Fadd(object->pos[1], object->bbox[0][1]), climb)))
                    continue;

                y = Fmin(y, Fsub(Fadd(object->pos[1], object->bbox[0][1]), displacee->bbox[1][1]));
                stop = true;
            }
            displacee->values[VAL_Y_TOUCH] = stop;
        }

        if (stop)
            displacee->values[VAL_Y_SPEED] = FxZero;
    }

    move_object(did, (fvec2){x, y});
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

    state.size[0] = Fdouble(F_SCREEN_WIDTH);
    state.size[1] = F_SCREEN_HEIGHT;

    for (size_t i = 0; i < num_players; i++) {
        struct GamePlayer* player = &(state.players[i]);
        player->active = true;

        player->bounds[0][0] = player->bounds[0][1] = FxZero;
        player->bounds[1][0] = state.size[0];
        player->bounds[1][1] = state.size[1];

        player->lives = 4L;

        ObjectID object = create_object(OBJ_PLAYER, (fvec2){FfInt(64L), FfInt(240L)});
        if (object_is_alive(object))
            state.objects[object].values[VAL_PLAYER_INDEX] = player->object = object;
        player->object = object;
    }

    add_gradient(
        0, 0, 11008, 551, 200,
        (GLubyte[4][4]){{0, 111, 223, 255}, {0, 111, 223, 255}, {242, 253, 252, 255}, {242, 253, 252, 255}}
    );
    add_backdrop(TEX_GRASS1, 0, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 32, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 64, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 96, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 128, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 160, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS3, 192, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS4, 0, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 32, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 64, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 96, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 128, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 160, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS6, 192, 448, 20, 255, 255, 255, 255);

    add_backdrop(TEX_BRIDGE2, 32, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 64, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 96, 240, 20, 255, 255, 255, 255);

    create_object(OBJ_BUSH, (fvec2){FfInt(-16L), FfInt(400L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(128L), FfInt(32L)});

    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(32L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(32L)});
    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(64L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(64L)});
    create_object(OBJ_COIN, (fvec2){FfInt(32L), FfInt(96L)});
    create_object(OBJ_COIN, (fvec2){FfInt(64L), FfInt(96L)});

    ObjectID oid = create_object(OBJ_SOLID, (fvec2){FxZero, FfInt(416L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(224L);
        state.objects[oid].bbox[1][1] = FfInt(128L);
    }
    oid = create_object(OBJ_SOLID_TOP, (fvec2){FfInt(32L), FfInt(240L)});
    if (object_is_alive(oid)) {
        state.objects[oid].bbox[1][0] = FfInt(96L);
        state.objects[oid].bbox[1][1] = FfInt(16L);
    }

    load_track(MUS_OVERWORLD1);
    play_track(MUS_OVERWORLD1, true);
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
    switch (state.objects[other].type) {
        default:
            break;

        case OBJ_PLAYER:
            state.objects[self].flags |= FLG_PLAYER_COLLISION;
            break;

        case OBJ_COIN: {
            ++(state.players[state.objects[self].values[VAL_PLAYER_INDEX]].coins);
            state.objects[other].object_flags |= OBF_DESTROY;
            play_sound(SND_COIN);
            break;
        }
    }
    return true;
}

static bool bullet_collide(ObjectID self, ObjectID other) {
    struct GameObject* hit = &(state.objects[other]);
    if (hit->type != OBJ_BULLET &&
        (hit->type != OBJ_PLAYER || hit->values[VAL_PLAYER_INDEX] != state.objects[self].values[VAL_BULLET_PLAYER])) {
        state.objects[self].values[VAL_BULLET_FRAME] += 24L;
        play_sound(SND_BUMP);
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
                fix16_t pid = object->values[VAL_PLAYER_INDEX];
                if (pid < 0L || pid >= MAX_PLAYERS || !(state.players[pid].active)) {
                    object->object_flags |= OBF_DESTROY;
                    break;
                }
                struct GamePlayer* player = &(state.players[pid]);

                object->values[VAL_X_SPEED] = Fadd(
                    Fmul(object->values[VAL_X_SPEED], 0x0000E666),
                    FfInt((int8_t)player->input.action.right - (int8_t)player->input.action.left)
                );
                object->values[VAL_Y_SPEED] = Fadd(
                    Fadd(object->values[VAL_Y_SPEED], 0x0000A666),
                    FfInt((int8_t)player->input.action.down - (int8_t)player->input.action.up)
                );

                if (player->input.action.jump && !(player->last_input.action.jump)) {
                    object->values[VAL_Y_SPEED] = FfInt(-16L);
                    play_sound(SND_JUMP);
                }

                if (object->values[VAL_X_SPEED] > FxZero)
                    object->object_flags &= ~OBF_X_FLIP;
                else if (object->values[VAL_X_SPEED] < FxZero)
                    object->object_flags |= OBF_X_FLIP;

                if ((object->pos[0] <= Fsub(player->bounds[0][0], object->bbox[0][0]) &&
                     object->values[VAL_X_SPEED] < FxZero) ||
                    (object->pos[0] >= Fsub(player->bounds[1][0], object->bbox[1][0]) &&
                     object->values[VAL_X_SPEED] > FxZero))
                    object->values[VAL_X_SPEED] = FxZero;
                if ((object->pos[1] <= Fsub(player->bounds[0][1], object->bbox[0][1]) &&
                     object->values[VAL_Y_SPEED] < FxZero) ||
                    (object->pos[1] >= Fsub(player->bounds[1][1], object->bbox[1][1]) &&
                     object->values[VAL_Y_SPEED] > FxZero))
                    object->values[VAL_Y_SPEED] = FxZero;

                displace_object(oid, FfInt(10L), true);

                if (player->input.action.fire && !(player->last_input.action.fire) &&
                    (Fabs(object->values[VAL_X_SPEED]) >= FxHalf || Fabs(object->values[VAL_Y_SPEED]) >= FxHalf)) {
                    ObjectID bullet_id = create_object(OBJ_BULLET, object->pos);
                    if (object_is_alive(bullet_id)) {
                        struct GameObject* bullet = &(state.objects[bullet_id]);
                        bullet->values[VAL_BULLET_PLAYER] = object->values[VAL_PLAYER_INDEX];
                        bullet->values[VAL_X_SPEED] = Fmul(object->values[VAL_X_SPEED], FfInt(2L));
                        bullet->values[VAL_Y_SPEED] = Fmul(object->values[VAL_Y_SPEED], FfInt(2L));
                        play_sound(SND_FIRE);
                    }
                }

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

                fix16_t pid = object->values[VAL_PLAYER_INDEX];
                if (pid < 0L || pid >= MAX_PLAYERS || !(state.players[pid].active)) {
                    object->object_flags |= OBF_DESTROY;
                    break;
                }
                struct GamePlayer* player = &(state.players[pid]);

                move_object(
                    oid, (fvec2){Fadd(object->pos[0], object->values[VAL_X_SPEED]),
                                 Fadd(object->pos[1], object->values[VAL_Y_SPEED])}
                );
                if (object->pos[0] < player->bounds[0][0] || object->pos[1] < player->bounds[0][1] ||
                    object->pos[0] > player->bounds[1][0] || object->pos[1] > player->bounds[1][1]) {
                    object->values[VAL_BULLET_FRAME] += 24L;
                    play_sound(SND_BUMP);
                } else {
                    iterate_block(oid, bullet_collide);
                }
                break;
            }
        }

        ObjectID next = object->previous;
        if (object->object_flags & OBF_DESTROY)
            destroy_object(oid);
        oid = next;
    }

    ++(state.time);

    struct GamePlayer* player = &(state.players[local_player]);
    if (object_is_alive(player->object)) {
        struct GameObject* pawn = &(state.objects[player->object]);
        const float cx = FtFloat(pawn->pos[0]);
        const float cy = FtFloat(pawn->pos[1]);
        const float bx1 = FtFloat(player->bounds[0][0]) + HALF_SCREEN_WIDTH;
        const float by1 = FtFloat(player->bounds[0][1]) + HALF_SCREEN_HEIGHT;
        const float bx2 = FtFloat(player->bounds[1][0]) - HALF_SCREEN_WIDTH;
        const float by2 = FtFloat(player->bounds[1][1]) - HALF_SCREEN_HEIGHT;
        move_camera(SDL_clamp(cx, bx1, bx2), SDL_clamp(cy, by1, by2));
    }
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
                    switch ((int)((float)state.time / 12.5f) % 3) {
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
                    switch ((int)((float)state.time / 7.142857142857143f) % 3) {
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

                case OBJ_COIN: {
                    enum TextureIndices tex;
                    switch ((state.time / 5) % 3) {
                        default:
                            tex = TEX_COIN1;
                            break;
                        case 1:
                            tex = TEX_COIN2;
                            break;
                        case 2:
                            tex = TEX_COIN3;
                            break;
                    }
                    draw_object(object, tex);
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
                    load_sound(SND_JUMP);
                    load_sound(SND_FIRE);

                    object->bbox[0][0] = FfInt(-9L);
                    object->bbox[0][1] = FfInt(-25L);
                    object->bbox[1][0] = FfInt(10L);
                    object->bbox[1][1] = FxOne;
                    object->depth = -1L;

                    object->values[VAL_PLAYER_INDEX] = -1L;
                    object->values[VAL_X_SPEED] = FxZero;
                    object->values[VAL_Y_SPEED] = FxZero;

                    object->flags = FLG_PLAYER_DEFAULT;
                    break;
                }

                case OBJ_BULLET: {
                    load_texture(TEX_BULLET);
                    load_texture(TEX_BULLET_HIT1);
                    load_texture(TEX_BULLET_HIT2);
                    load_texture(TEX_BULLET_HIT3);
                    load_sound(SND_BUMP);

                    object->bbox[0][0] = FfInt(-5L);
                    object->bbox[0][1] = FfInt(-5L);
                    object->bbox[1][0] = FfInt(5L);
                    object->bbox[1][1] = FfInt(5L);

                    object->values[VAL_BULLET_PLAYER] = -1L;
                    object->values[VAL_X_SPEED] = FxZero;
                    object->values[VAL_Y_SPEED] = FxZero;
                    object->values[VAL_BULLET_FRAME] = 0L;
                    break;
                }

                case OBJ_COIN: {
                    load_texture(TEX_COIN1);
                    load_texture(TEX_COIN2);
                    load_texture(TEX_COIN3);
                    load_sound(SND_COIN);

                    object->bbox[0][0] = FfInt(6L);
                    object->bbox[0][1] = FfInt(2L);
                    object->bbox[1][0] = FfInt(25L);
                    object->bbox[1][1] = FfInt(30L);
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

const struct BlockList* list_block_at(const fvec2 rect[2]) {
    static struct BlockList list = {0};
    list.num_objects = 0;

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
                struct GameObject* other = &(state.objects[oid]);
                fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                if (rect[0][0] < ox2 && rect[1][0] > ox1 && rect[0][1] < oy2 && rect[1][1] > oy1)
                    list.objects[list.num_objects++] = oid;
                oid = other->previous_block;
            }
        }

    return &list;
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
                    if (x1 < ox2 && x2 > ox1 && y1 < oy2 && y2 > oy1 && !iterator(index, oid))
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

int32_t random() {
    // https://rosettacode.org/wiki/Linear_congruential_generator
    state.seed = (state.seed * 214013 + 2531011) & ((1U << 31) - 1);
    return (int32_t)state.seed >> 16;
}
