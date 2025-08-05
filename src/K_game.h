#pragma once

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>

#include "K_math.h"
#include "K_net.h" // IWYU pragma: keep

#define MAX_PLAYERS 4L

#define MAX_OBJECTS 1000L
#define MAX_VALUES 32L

#define MAX_BLOCKS 128L
#define BLOCKMAP_SIZE (MAX_BLOCKS * MAX_BLOCKS)
#define BLOCK_SIZE ((fix16_t)0x00800000)

#define SCREEN_WIDTH 640L
#define SCREEN_HEIGHT 480L
#define F_SCREEN_WIDTH FfInt(SCREEN_WIDTH)
#define F_SCREEN_HEIGHT FfInt(SCREEN_HEIGHT)

typedef int16_t ObjectID;

enum GameObjectType {
    OBJ_INVALID,
    OBJ_PLAYER,
    OBJ_BULLET,
};

enum ObjectFlags {
    OBF_NONE = 0,
    OBF_VISIBLE = 1 << 0,
    OBF_DESTROY = 1 << 1,
    OBF_X_FLIP = 1 << 2,
    OBF_Y_FLIP = 1 << 3,

    OBF_DEFAULT = OBF_VISIBLE,
};

enum PlayerValues {
    VAL_PLAYER_INDEX,
    VAL_PLAYER_X_SPEED,
    VAL_PLAYER_Y_SPEED,
};

enum PlayerFlags {
    FLG_PLAYER_NONE = 0,
    FLG_PLAYER_COLLISION = 1 << 0,
    FLG_PLAYER_DEFAULT = FLG_PLAYER_NONE,
};

enum BulletValues {
    VAL_BULLET_PLAYER,
    VAL_BULLET_X_SPEED,
    VAL_BULLET_Y_SPEED,
};

struct GameInput {
    union {
        struct {
            bool up : 1;
            bool left : 1;
            bool down : 1;
            bool right : 1;
            bool jump : 1;
            bool run : 1;
            bool fire : 1;
        } action;
        uint8_t value;
    };
};

struct GameState {
    struct GamePlayer {
        bool active;
        struct GameInput input, last_input;
        ObjectID object;
    } players[MAX_PLAYERS];

    uint32_t seed;

    struct GameObject {
        enum GameObjectType type;
        enum ObjectFlags object_flags;
        ObjectID previous, next;

        int32_t block;
        ObjectID previous_block, next_block;

        fvec2 pos;
        fvec2 bbox[2];

        fix16_t values[MAX_VALUES];
        uint32_t flags;
    } objects[MAX_OBJECTS];
    ObjectID live_objects;
    ObjectID next_object;

    ObjectID blockmap[BLOCKMAP_SIZE];
};

void start_state(int, int);
void save_state(GekkoGameEvent*);
void load_state(GekkoGameEvent*);
void tick_state(struct GameInput[MAX_PLAYERS]);
void draw_state(SDL_Renderer*);

bool object_is_alive(ObjectID);
ObjectID create_object(enum GameObjectType, const fvec2);
void move_object(ObjectID, const fvec2);
void iterate_block(ObjectID, bool (*iterator)(ObjectID, ObjectID));
void destroy_object(ObjectID);
