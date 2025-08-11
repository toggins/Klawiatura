#include "K_game.h"
#include "K_audio.h"
#include "K_log.h"

static struct GameState state = {0};
static int local_player = -1L;

/* ====

   GAME

   ==== */

static void give_points(struct GameObject* item, int player, int points) {
    if (points < 0L) {
        state.players[player].lives -= points;
        if (player == local_player)
            play_sound(SND_1UP);
    } else
        state.players[player].score += points;

    const ObjectID pid = create_object(OBJ_POINTS, (fvec2){item->pos[0], Fadd(item->pos[1], item->bbox[0][1])});
    if (object_is_alive(pid)) {
        state.objects[pid].values[VAL_POINTS_PLAYER] = player;
        state.objects[pid].values[VAL_POINTS] = points;
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
                if (other->type == OBJ_SOLID) {
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
                const struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
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
            for (size_t i = 0; i < list->num_objects; i++) {
                const struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
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
    list = list_block_at((fvec2[2]){
        {Fadd(x, displacee->bbox[0][0]), Fadd(y, displacee->bbox[0][1])},
        {Fadd(x, displacee->bbox[1][0]), Fadd(y, displacee->bbox[1][1])},
    });

    if (list->num_objects > 0) {
        bool stop = false;
        if (displacee->values[VAL_Y_SPEED] < FxZero) {
            for (size_t i = 0; i < list->num_objects; i++) {
                const struct GameObject* object = &(state.objects[list->objects[i]]);
                if (object->type != OBJ_SOLID)
                    continue;

                y = Fmax(y, Fsub(Fadd(object->pos[1], object->bbox[1][1]), displacee->bbox[0][1]));
                stop = true;
            }
            displacee->values[VAL_Y_TOUCH] = -stop;
        } else if (displacee->values[VAL_Y_SPEED] > FxZero) {
            for (size_t i = 0; i < list->num_objects; i++) {
                const struct GameObject* object = &(state.objects[list->objects[i]]);
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

static bool bump_check(ObjectID self_id, ObjectID other_id) {
    struct GameObject* self = &(state.objects[self_id]);
    switch (self->type) {
        default:
            break;

        case OBJ_COIN: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (++(player->coins) >= 100L) {
                    give_points(self, pid, -1L);
                    player->coins = 0L;
                }

                player->score += 200L;
                play_sound_at_object(self, SND_COIN);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }

        case OBJ_MUSHROOM: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (player->power == POW_SMALL) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }

        case OBJ_FIRE_FLOWER: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                } else if (player->power != POW_FIRE) {
                    other->values[VAL_PLAYER_POWER] = 4000L;
                    player->power = POW_FIRE;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }

        case OBJ_BEETROOT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                } else if (player->power != POW_BEETROOT) {
                    other->values[VAL_PLAYER_POWER] = 4000L;
                    player->power = POW_BEETROOT;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }

        case OBJ_LUI: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                } else if (player->power != POW_LUI) {
                    other->values[VAL_PLAYER_POWER] = 4000L;
                    player->power = POW_LUI;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }

        case OBJ_HAMMER_SUIT: {
            struct GameObject* other = &(state.objects[other_id]);
            if (other->type == OBJ_PLAYER) {
                const int pid = other->values[VAL_PLAYER_INDEX];
                struct GamePlayer* player = &(state.players[pid]);
                if (player->power == POW_SMALL && !(self->flags & FLG_POWERUP_FULL)) {
                    other->values[VAL_PLAYER_POWER] = 3000L;
                    player->power = POW_BIG;
                } else if (player->power != POW_HAMMER) {
                    other->values[VAL_PLAYER_POWER] = 4000L;
                    player->power = POW_HAMMER;
                }
                give_points(self, pid, 1000L);

                play_sound_at_object(other, SND_GROW);
                self->object_flags |= OBF_DESTROY;
            }
            break;
        }
    }

    return true;
}

static void bump_object(ObjectID bid) {
    if (!object_is_alive(bid))
        return;

    struct GameObject* object = &(state.objects[bid]);
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
                if (bid != oid) {
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

static enum PlayerFrames get_player_frame(struct GameObject* object) {
    if (object->values[VAL_PLAYER_POWER] > 0L)
        switch (state.players[object->values[VAL_PLAYER_INDEX]].power) {
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

    if (object->values[VAL_PLAYER_GROUND] <= 0L)
        return (object->values[VAL_Y_SPEED] < FxZero) ? PF_JUMP : PF_FALL;

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

static bool in_any_view(struct GameObject* object, fix16_t padding) {
    const fix16_t sx1 = Fadd(object->pos[0], object->bbox[0][0]);
    const fix16_t sy1 = Fadd(object->pos[1], object->bbox[0][1]);
    const fix16_t sx2 = Fadd(object->pos[0], object->bbox[1][0]);
    const fix16_t sy2 = Fadd(object->pos[1], object->bbox[1][1]);

    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        const struct GamePlayer* player = &(state.players[i]);
        if (!(player->active && object_is_alive(player->object)))
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
        if (sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && sy2 > oy1)
            return true;
    }

    return false;
}

static bool in_player_view(struct GameObject* object, int pid, fix16_t padding) {
    const struct GamePlayer* player = &(state.players[pid]);
    if (!(player->active && object_is_alive(player->object)))
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

    return sx1 < ox2 && sx2 > ox1 && sy1 < oy2 && sy2 > oy1;
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
    add_backdrop(TEX_GRASS2, 192, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 224, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 256, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 288, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 320, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 352, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 384, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 416, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 448, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 480, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 512, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 544, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS2, 576, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS3, 608, 416, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS4, 0, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 32, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 64, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 96, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 128, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 160, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 192, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 224, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 256, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 288, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 320, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 352, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 384, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 416, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 448, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 480, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 512, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 544, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS5, 576, 448, 20, 255, 255, 255, 255);
    add_backdrop(TEX_GRASS6, 608, 448, 20, 255, 255, 255, 255);

    add_backdrop(TEX_BRIDGE2, 32, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 64, 240, 20, 255, 255, 255, 255);
    add_backdrop(TEX_BRIDGE2, 96, 240, 20, 255, 255, 255, 255);

    create_object(OBJ_BUSH, (fvec2){FfInt(384L), FfInt(384L)});
    create_object(OBJ_CLOUD, (fvec2){FfInt(128L), FfInt(32L)});

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
    INFO("  Clock: %u", state.clock);
    INFO("  Time: %zu", state.time);
    INFO("  Seed: %u", state.seed);

    INFO("\n[OBJECTS]");
    ObjectID oid = state.live_objects;
    while (object_is_alive(oid)) {
        const struct GameObject* object = &(state.objects[oid]);

        INFO("  Object %i:", oid);
        INFO("      Type: %i", object->type);
        INFO("      Object Flags: %u", object->object_flags);
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

                const bool cant_run = !(player->input & GI_RUN);
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
                        object->values[VAL_X_SPEED], object->values[VAL_X_SPEED] >= FxZero ? 0x00002000 : -0x00002000
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

                if ((player->input & GI_JUMP) && object->values[VAL_Y_SPEED] < FxZero /* and _y < _water_y */ &&
                    !(player->input & GI_DOWN))
                    object->values[VAL_Y_SPEED] = Fsub(
                        object->values[VAL_Y_SPEED],
                        (player->power == POW_LUI)
                            ? 0x0000999A
                            : ((Fabs(object->values[VAL_X_SPEED]) < 0x0000A000) ? 0x00006666 : FxHalf)
                    );

                if ((jumped || ((player->input & GI_JUMP) && object->flags & FLG_PLAYER_JUMP)) &&
                    /* _y < _water_y and */ !(player->input & GI_DOWN) && object->values[VAL_PLAYER_GROUND] > 0L) {
                    object->values[VAL_PLAYER_GROUND] = 0L;
                    object->values[VAL_Y_SPEED] = FfInt(-13L);
                    object->flags &= ~FLG_PLAYER_JUMP;
                    play_sound_at_object(object, SND_JUMP);
                }
                if (!(player->input & GI_JUMP) && /* _y < _water_y and */ !(player->input & GI_DOWN) &&
                    object->values[VAL_PLAYER_GROUND] > 0L && (object->flags & FLG_PLAYER_JUMP))
                    object->flags &= ~FLG_PLAYER_JUMP;
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
                    const bool carried = object->object_flags & OBF_CARRIED;
                    if (object->values[VAL_Y_SPEED] > FfInt(10L) && !carried) {
                        object->values[VAL_Y_SPEED] = Fmin(Fsub(object->values[VAL_Y_SPEED], FxOne), FfInt(10L));
                    } else if (object->values[VAL_Y_SPEED] < FfInt(10L) || carried) {
                        object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], FxOne);
                        object->object_flags &= ~OBF_CARRIED;
                    }
                }

                // Animation
                if (object->values[VAL_X_SPEED] > FxZero)
                    object->object_flags &= ~OBF_X_FLIP;
                else if (object->values[VAL_X_SPEED] < FxZero)
                    object->object_flags |= OBF_X_FLIP;

                if (object->values[VAL_PLAYER_POWER] > 0L) {
                    object->values[VAL_PLAYER_POWER] -= 91L;
                    object->values[VAL_PLAYER_FRAME] = FxZero;
                }
                if (object->values[VAL_PLAYER_FIRE] > 0L) {
                    --(object->values[VAL_PLAYER_FIRE]);
                    object->values[VAL_PLAYER_FRAME] = FxZero;
                }

                object->values[VAL_PLAYER_FRAME] =
                    (object->values[VAL_PLAYER_GROUND] > 0L && object->values[VAL_X_SPEED] != FxZero)
                        ? Fadd(
                              object->values[VAL_PLAYER_FRAME],
                              Fclamp(Fmul(Fabs(object->values[VAL_X_SPEED]), 0x0000147B), 0x000023D7, FxHalf)
                          )
                        : FxZero;

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
                                    const ObjectID mid = create_object(
                                        OBJ_MISSILE_FIREBALL,
                                        (fvec2){
                                            Fadd(object->pos[0], FfInt((object->object_flags & OBF_X_FLIP) ? -5L : 5L)),
                                            Fsub(object->pos[1], FfInt(29L))
                                        }
                                    );
                                    if (object_is_alive(mid)) {
                                        object->values[i] = mid;
                                        struct GameObject* missile = &(state.objects[mid]);
                                        missile->object_flags |= object->object_flags & OBF_X_FLIP;
                                        missile->values[VAL_MISSILE_OWNER] = oid;
                                        missile->values[VAL_X_SPEED] =
                                            (object->object_flags & OBF_X_FLIP) ? -0x0007C000 : 0x0007C000;
                                    }

                                    if (object->values[VAL_PLAYER_GROUND] > 0L)
                                        object->values[VAL_PLAYER_FIRE] = 2L;
                                    play_sound_at_object(object, SND_FIRE);
                                    break;
                                }
                        break;
                    }

                    case POW_LUI: {
                        if (object->values[VAL_PLAYER_GROUND] <= 0L) {
                            const ObjectID eid = create_object(OBJ_PLAYER_EFFECT, object->pos);
                            if (object_is_alive(eid)) {
                                struct GameObject* effect = &(state.objects[eid]);
                                effect->object_flags |= object->object_flags;
                                effect->values[VAL_PLAYER_EFFECT_POWER] = player->power;
                                effect->values[VAL_PLAYER_EFFECT_FRAME] = get_player_frame(object);
                            }
                        }
                        break;
                    }
                }

                bump_object(oid);
                break;
            }

            case OBJ_PLAYER_EFFECT: {
                object->values[VAL_PLAYER_EFFECT_ALPHA] = Fsub(object->values[VAL_PLAYER_EFFECT_ALPHA], 0x0009F600);
                if (object->values[VAL_PLAYER_EFFECT_ALPHA] <= FxZero)
                    object->object_flags |= OBF_DESTROY;
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

                if (object->values[VAL_POINTS] >= 10000L) {
                    if (time >= 300L)
                        object->object_flags |= OBF_DESTROY;
                } else if (object->values[VAL_POINTS] >= 1000L) {
                    if (time >= 200L)
                        object->object_flags |= OBF_DESTROY;
                } else if (time >= 100L)
                    object->object_flags |= OBF_DESTROY;

                break;
            }

            case OBJ_MISSILE_FIREBALL: {
                object->values[VAL_MISSILE_ANGLE] = Fadd(
                    object->values[VAL_MISSILE_ANGLE], object->values[VAL_X_SPEED] < FxZero ? -0x000B4000 : 0x000B4000
                );
                object->values[VAL_Y_SPEED] = Fadd(object->values[VAL_Y_SPEED], 0x00006666);

                displace_object(oid, FfInt(10L), false);
                if (object->values[VAL_X_TOUCH] != 0L) {
                    object->object_flags |= OBF_DESTROY;
                    break;
                }
                if (object->values[VAL_Y_TOUCH] > 0L)
                    object->values[VAL_Y_SPEED] = FfInt(-5L);

                const ObjectID ownid = (ObjectID)(object->values[VAL_MISSILE_OWNER]);
                if (object_is_alive(ownid) && state.objects[ownid].type == OBJ_PLAYER) {
                    if (!in_player_view(object, state.objects[ownid].values[VAL_PLAYER_INDEX], FxZero))
                        object->object_flags |= OBF_DESTROY;
                } else if (!in_any_view(object, FxZero))
                    object->object_flags |= OBF_DESTROY;

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
        const float cx = FtInt(pawn->pos[0]);
        const float cy = FtInt(pawn->pos[1]);
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
                    switch ((int)((float)state.time / 12.5f) % 4) {
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
                    draw_object(object, tex, 0, WHITE);
                    break;
                }

                case OBJ_BUSH: {
                    enum TextureIndices tex;
                    switch ((int)((float)state.time / 7.142857142857143f) % 4) {
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
                    draw_object(object, tex, 0, WHITE);
                    break;
                }

                case OBJ_PLAYER: {
                    draw_object(
                        object,
                        get_player_texture(
                            state.players[object->values[VAL_PLAYER_INDEX]].power, get_player_frame(object)
                        ),
                        0, WHITE
                    );
                    break;
                }

                case OBJ_PLAYER_EFFECT: {
                    draw_object(
                        object,
                        get_player_texture(
                            object->values[VAL_PLAYER_EFFECT_POWER], object->values[VAL_PLAYER_EFFECT_FRAME]
                        ),
                        0, ALPHA(FtInt(object->values[VAL_PLAYER_EFFECT_ALPHA]))
                    );
                    break;
                }

                case OBJ_COIN: {
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
                    draw_object(object, tex, 0, WHITE);
                    break;
                }

                case OBJ_MUSHROOM: {
                    draw_object(object, TEX_MUSHROOM, 0, WHITE);
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
                    draw_object(object, tex, 0, WHITE);
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
                    draw_object(object, tex, 0, WHITE);
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
                    draw_object(object, tex, 0, WHITE);
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    draw_object(object, TEX_HAMMER_SUIT, 0, WHITE);
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

                    draw_object(object, tex, 0, WHITE);
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    draw_object(object, TEX_MISSILE_FIREBALL, FtFloat(object->values[VAL_MISSILE_ANGLE]), WHITE);
                    break;
                }
            }

        oid = object->previous;
    }
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

            load_sound(SND_JUMP);
            load_sound(SND_FIRE);
            load_sound(SND_GROW);
            load_sound(SND_WARP);
            break;
        }

        case OBJ_COIN: {
            load_texture(TEX_COIN1);
            load_texture(TEX_COIN2);
            load_texture(TEX_COIN3);

            load_sound(SND_COIN);
            break;
        }

        case OBJ_MUSHROOM: {
            load_texture(TEX_MUSHROOM);
            break;
        }

        case OBJ_MUSHROOM_1UP: {
            load_texture(TEX_MUSHROOM_1UP);
            break;
        }

        case OBJ_MUSHROOM_POISON: {
            load_texture(TEX_MUSHROOM_POISON1);
            load_texture(TEX_MUSHROOM_POISON2);
            break;
        }

        case OBJ_FIRE_FLOWER: {
            load_texture(TEX_FIRE_FLOWER1);
            load_texture(TEX_FIRE_FLOWER2);
            load_texture(TEX_FIRE_FLOWER3);
            load_texture(TEX_FIRE_FLOWER4);
            break;
        }

        case OBJ_BEETROOT: {
            load_texture(TEX_BEETROOT1);
            load_texture(TEX_BEETROOT2);
            load_texture(TEX_BEETROOT3);
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
            break;
        }

        case OBJ_HAMMER_SUIT: {
            load_texture(TEX_HAMMER_SUIT);
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
            break;
        }
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
            load_object(type);
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

                case OBJ_CLOUD:
                case OBJ_BUSH: {
                    object->depth = 25L;
                    break;
                }

                case OBJ_PLAYER: {
                    object->depth = -1L;

                    object->bbox[0][0] = FfInt(-9L);
                    object->bbox[0][1] = FfInt(-25L);
                    object->bbox[1][0] = FfInt(10L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_X_SPEED] = object->values[VAL_Y_SPEED] = FxZero;
                    object->values[VAL_X_TOUCH] = object->values[VAL_Y_TOUCH] = 0L;
                    object->values[VAL_PLAYER_INDEX] = -1L;
                    object->values[VAL_PLAYER_FRAME] = FxZero;
                    object->values[VAL_PLAYER_POWER] = object->values[VAL_PLAYER_FIRE] = 0L;
                    for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                        object->values[i] = -1L;

                    object->flags = FLG_PLAYER_DEFAULT;
                    break;
                }

                case OBJ_PLAYER_EFFECT: {
                    object->values[VAL_PLAYER_EFFECT_POWER] = POW_SMALL;
                    object->values[VAL_PLAYER_EFFECT_FRAME] = PF_IDLE;
                    object->values[VAL_PLAYER_EFFECT_ALPHA] = FfInt(255L);
                    break;
                }

                case OBJ_COIN: {
                    object->depth = 1L;

                    object->bbox[0][0] = FfInt(6L);
                    object->bbox[0][1] = FfInt(2L);
                    object->bbox[1][0] = FfInt(25L);
                    object->bbox[1][1] = FfInt(30L);
                    break;
                }

                case OBJ_MUSHROOM: {
                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(15L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_X_SPEED] = object->values[VAL_Y_SPEED] = FxZero;
                    object->values[VAL_X_TOUCH] = object->values[VAL_Y_TOUCH] = 0L;

                    object->flags = FLG_POWERUP_DEFAULT;
                    break;
                }

                case OBJ_FIRE_FLOWER: {
                    object->bbox[0][0] = FfInt(-17L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(16L);
                    object->bbox[1][1] = FxOne;

                    object->flags = FLG_POWERUP_DEFAULT;
                    break;
                }

                case OBJ_BEETROOT: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-32L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags = FLG_POWERUP_DEFAULT;
                    break;
                }

                case OBJ_LUI: {
                    object->bbox[0][0] = FfInt(-15L);
                    object->bbox[0][1] = FfInt(-30L);
                    object->bbox[1][0] = FfInt(15L);
                    object->bbox[1][1] = FxOne;

                    object->values[VAL_X_SPEED] = object->values[VAL_Y_SPEED] = FxZero;
                    object->values[VAL_X_TOUCH] = object->values[VAL_Y_TOUCH] = 0L;
                    object->values[VAL_LUI_BOUNCE] = 0L;

                    object->flags = FLG_POWERUP_DEFAULT;
                    break;
                }

                case OBJ_HAMMER_SUIT: {
                    object->bbox[0][0] = FfInt(-13L);
                    object->bbox[0][1] = FfInt(-31L);
                    object->bbox[1][0] = FfInt(14L);
                    object->bbox[1][1] = FxOne;

                    object->flags = FLG_POWERUP_DEFAULT;
                    break;
                }

                case OBJ_POINTS: {
                    object->depth = -100L;

                    object->values[VAL_POINTS_PLAYER] = -1L;
                    object->values[VAL_POINTS] = 0L;
                    object->values[VAL_POINTS_TIME] = 0L;
                    break;
                }

                case OBJ_MISSILE_FIREBALL: {
                    object->bbox[0][0] = object->bbox[0][1] = FfInt(-7L);
                    object->bbox[1][0] = object->bbox[1][1] = FfInt(8L);

                    object->values[VAL_X_SPEED] = object->values[VAL_Y_SPEED] = FxZero;
                    object->values[VAL_X_TOUCH] = object->values[VAL_Y_TOUCH] = 0L;
                    object->values[VAL_MISSILE_OWNER] = -1;
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
                const struct GameObject* other = &(state.objects[oid]);
                const fix16_t ox1 = Fadd(other->pos[0], other->bbox[0][0]);
                const fix16_t oy1 = Fadd(other->pos[1], other->bbox[0][1]);
                const fix16_t ox2 = Fadd(other->pos[0], other->bbox[1][0]);
                const fix16_t oy2 = Fadd(other->pos[1], other->bbox[1][1]);
                if (rect[0][0] < ox2 && rect[1][0] > ox1 && rect[0][1] < oy2 && rect[1][1] > oy1)
                    list.objects[list.num_objects++] = oid;
                oid = other->previous_block;
            }
        }

    return &list;
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

            for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++) {
                const ObjectID mid = (ObjectID)(object->values[i]);
                if (object_is_alive(mid))
                    state.objects[mid].values[VAL_MISSILE_OWNER] = -1L;
            }

            break;
        }

        case OBJ_MISSILE_FIREBALL:
        case OBJ_MISSILE_BEETROOT:
        case OBJ_MISSILE_HAMMER: {
            const ObjectID oid = (ObjectID)(object->values[VAL_MISSILE_OWNER]);
            if (object_is_alive(oid)) {
                struct GameObject* owner = &(state.objects[oid]);
                if (owner->type != OBJ_PLAYER)
                    break;

                for (size_t i = VAL_PLAYER_MISSILE_START; i < VAL_PLAYER_MISSILE_END; i++)
                    if (owner->values[i] == index) {
                        owner->values[i] = -1L;
                        break;
                    }
            }
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

void draw_object(struct GameObject* object, enum TextureIndices tid, GLfloat angle, const GLubyte color[4]) {
    draw_sprite(
        tid, (float[3]){FtInt(object->pos[0]), FtInt(object->pos[1]), object->depth},
        (bool[2]){object->object_flags & OBF_X_FLIP, object->object_flags & OBF_Y_FLIP}, angle, color
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
