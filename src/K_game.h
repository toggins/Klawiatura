#pragma once

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>

#include "K_audio.h"
#include "K_math.h"
#include "K_video.h" // IWYU pragma: keep

#define TICKRATE 50L

#define MAX_PLAYERS 4L

#define MAX_OBJECTS 1000L
#define MAX_VALUES 32L

#define MAX_BLOCKS 128L
#define BLOCKMAP_SIZE (MAX_BLOCKS * MAX_BLOCKS)
#define BLOCK_SIZE ((fix16_t)0x00800000)

#define SCREEN_WIDTH 640L
#define SCREEN_HEIGHT 480L
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH >> 1)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT >> 1)
#define F_SCREEN_WIDTH FfInt(SCREEN_WIDTH)
#define F_SCREEN_HEIGHT FfInt(SCREEN_HEIGHT)
#define F_HALF_SCREEN_WIDTH Fhalf(F_SCREEN_WIDTH)
#define F_HALF_SCREEN_HEIGHT Fhalf(F_SCREEN_HEIGHT)

typedef int16_t ObjectID;

enum GameObjectType {
    OBJ_NULL,

    OBJ_SOLID,
    OBJ_SOLID_TOP,
    OBJ_CLOUD,
    OBJ_BUSH,
    OBJ_BUSH_SNOW,
    OBJ_PLAYER_SPAWN,
    OBJ_PLAYER,
    OBJ_PLAYER_EFFECT,
    OBJ_PLAYER_DEAD,
    OBJ_COIN,
    OBJ_COIN_POP,
    OBJ_MUSHROOM,
    OBJ_MUSHROOM_1UP,
    OBJ_MUSHROOM_POISON,
    OBJ_FIRE_FLOWER,
    OBJ_BEETROOT,
    OBJ_LUI,
    OBJ_HAMMER_SUIT,
    OBJ_STARMAN,
    OBJ_POINTS,
    OBJ_EXPLODE,
    OBJ_MISSILE_FIREBALL,
    OBJ_MISSILE_BEETROOT,
    OBJ_MISSILE_BEETROOT_SINK,
    OBJ_MISSILE_HAMMER,
    OBJ_BLOCK_BUMP,
    OBJ_ITEM_BLOCK,
    OBJ_HIDDEN_BLOCK,
    OBJ_BRICK_BLOCK,
    OBJ_BRICK_SHARD,
    OBJ_BRICK_BLOCK_COIN,
    OBJ_CHECKPOINT,
    OBJ_GOAL_BAR,
    OBJ_GOAL_BAR_FLY,
    OBJ_GOAL_MARK,
    OBJ_WATER_SPLASH,
    OBJ_BUBBLE,
    OBJ_DEAD,
    OBJ_GOOMBA,
    OBJ_GOOMBA_FLAT,
    OBJ_KOOPA,
    OBJ_KOOPA_SHELL,
    OBJ_PARAKOOPA,
    OBJ_BUZZY_BEETLE,
    OBJ_BUZZY_SHELL,
    OBJ_SPINY,
    OBJ_LAKITU,
    OBJ_SPIKE,
    OBJ_PIRANHA_PLANT,
    OBJ_BRO,
    OBJ_BRO_HAMMER,
    OBJ_BRO_FIREBALL,
    OBJ_BRO_SILVER_HAMMER,
    OBJ_BRO_LAYER,
    OBJ_BILL_BLASTER,
    OBJ_BULLET_BILL,
    OBJ_CHEEP_CHEEP,
    OBJ_CHEEP_CHEEP_GREEN,
    OBJ_CHEEP_CHEEP_BLUE,
    OBJ_CHEEP_CHEEP_SPIKY,
    OBJ_BLOOPER,
    OBJ_ROTODISC_BALL,
    OBJ_ROTODISC,
    OBJ_THWOMP,
    OBJ_BOO,
    OBJ_DRY_BONES,
    OBJ_LAVA,
    OBJ_PODOBOO_SPAWNER,
    OBJ_PODOBOO,
    OBJ_SPRING,
    OBJ_PSWITCH,
    OBJ_PSWITCH_COIN,
    OBJ_PSWITCH_BRICK,

    OBJ_SIZE,
};

enum ObjectValues {
    VAL_X_SPEED,
    VAL_Y_SPEED,
    VAL_X_TOUCH,
    VAL_Y_TOUCH,
    VAL_SPROUT,
    VAL_START,

    VAL_PROP_FRAME = VAL_START,

    VAL_PLAYER_INDEX = VAL_START,
    VAL_PLAYER_FRAME,
    VAL_PLAYER_GROUND,
    VAL_PLAYER_SPRING,
    VAL_PLAYER_POWER,
    VAL_PLAYER_FLASH,
    VAL_PLAYER_STARMAN,
    VAL_PLAYER_FIRE,
    VAL_PLAYER_MISSILE_START,
    VAL_PLAYER_MISSILE1 = VAL_PLAYER_MISSILE_START,
    VAL_PLAYER_MISSILE2,
    VAL_PLAYER_MISSILE_END,
    VAL_PLAYER_BEETROOT_START = VAL_PLAYER_MISSILE_END,
    VAL_PLAYER_BEETROOT1 = VAL_PLAYER_BEETROOT_START,
    VAL_PLAYER_BEETROOT2,
    VAL_PLAYER_BEETROOT3,
    VAL_PLAYER_BEETROOT4,
    VAL_PLAYER_BEETROOT5,
    VAL_PLAYER_BEETROOT6,
    VAL_PLAYER_BEETROOT_END,

    VAL_PLAYER_EFFECT_POWER = VAL_START,
    VAL_PLAYER_EFFECT_FRAME,
    VAL_PLAYER_EFFECT_ALPHA,

    VAL_LUI_BOUNCE = VAL_START,

    VAL_MISSILE_OWNER = VAL_START,
    VAL_MISSILE_ANGLE,
    VAL_MISSILE_HITS,
    VAL_MISSILE_HIT,
    VAL_MISSILE_BUBBLE,

    VAL_POINTS = VAL_START,
    VAL_POINTS_PLAYER,
    VAL_POINTS_TIME,

    VAL_EXPLODE_FRAME = VAL_START,

    VAL_BLOCK_BUMP = VAL_START,
    VAL_BLOCK_BUMP_OWNER,
    VAL_BLOCK_ITEM,
    VAL_BLOCK_TIME,

    VAL_BRICK_SHARD_ANGLE = VAL_START,

    VAL_COIN_POP_OWNER = VAL_START,
    VAL_COIN_POP_FRAME,

    VAL_ROTODISC = VAL_START,
    VAL_ROTODISC_OWNER = VAL_START,
    VAL_ROTODISC_LENGTH,
    VAL_ROTODISC_ANGLE,
    VAL_ROTODISC_SPEED,

    VAL_CHECKPOINT_BOUNDS_X1 = VAL_START,
    VAL_CHECKPOINT_BOUNDS_Y1,
    VAL_CHECKPOINT_BOUNDS_X2,
    VAL_CHECKPOINT_BOUNDS_Y2,

    VAL_WATER_SPLASH_FRAME = VAL_START,

    VAL_BUBBLE_FRAME = VAL_START,

    VAL_GOAL_Y = VAL_START,
    VAL_GOAL_ANGLE,

    VAL_PSWITCH = VAL_START,
};

enum ObjectFlags {
    FLG_VISIBLE = 1 << 0,
    FLG_DESTROY = 1 << 1,
    FLG_X_FLIP = 1 << 2,
    FLG_Y_FLIP = 1 << 3,
    FLG_CARRIED = 1 << 4,

    FLG_PLAYER_DUCK = 1 << 5,
    FLG_PLAYER_JUMP = 1 << 6,
    FLG_PLAYER_SWIM = 1 << 7,

    FLG_PLAYER_DEAD_LAST = 1 << 5,

    FLG_POWERUP_FULL = 1 << 5,

    FLG_BLOCK_EMPTY = 1 << 5,
    FLG_BLOCK_GRAY = 1 << 6,
    FLG_BLOCK_ONCE = 1 << 7,

    FLG_COIN_POP_START = 1 << 5,
    FLG_COIN_POP_SPARK = 1 << 6,

    FLG_ROTODISC_START = 1 << 5,
    FLG_ROTODISC_FLOWER = 1 << 6,
    FLG_ROTODISC_FLOWER2 = 1 << 7,

    FLG_BUBBLE_POP = 1 << 5,

    FLG_GOAL_START = 1 << 5,

    FLG_PSWITCH_ONCE = 1 << 5,
};

enum PlayerFrames {
    PF_IDLE,
    PF_WALK1,
    PF_WALK2,
    PF_WALK3,
    PF_JUMP,
    PF_FALL,
    PF_DUCK,
    PF_FIRE,
    PF_SWIM1,
    PF_SWIM2,
    PF_SWIM3,
    PF_SWIM4,
    PF_SWIM5,
    PF_GROW1,
    PF_GROW2,
    PF_GROW3,
    PF_GROW4,
};

enum GameInput {
    GI_NONE = 0,
    GI_UP = 1 << 0,
    GI_LEFT = 1 << 1,
    GI_DOWN = 1 << 2,
    GI_RIGHT = 1 << 3,
    GI_JUMP = 1 << 4,
    GI_RUN = 1 << 5,
    GI_FIRE = 1 << 6,
};

struct GameState {
    struct GamePlayer {
        bool active;
        enum GameInput input, last_input;

        ObjectID object;
        fvec2 bounds[2];

        uint8_t lives, coins;
        uint32_t score;
        enum PlayerPowers {
            POW_SMALL,
            POW_BIG,
            POW_FIRE,
            POW_BEETROOT,
            POW_LUI,
            POW_HAMMER,
        } power;
    } players[MAX_PLAYERS];

    enum GameFlags {
        GF_HURRY = 1 << 0,
        GF_END = 1 << 1,
        GF_SCROLL = 1 << 2,
        GF_LAVA = 1 << 3,
        GF_HARDCORE = 1 << 4,
        GF_SPIKES = 1 << 5,
        GF_FUNNY = 1 << 6,
        GF_LOST = 1 << 7,
        GF_KEVIN = 1 << 8,
    } flags;

    fvec2 size;
    fvec2 bounds[2];

    uint64_t time;
    uint32_t seed;

    int32_t clock;
    ObjectID spawn, checkpoint;
    fvec2 scroll;
    fix16_t water, hazard;
    uint16_t pswitch;

    struct GameSequence {
        enum GameSequenceType {
            SEQ_NONE,
            SEQ_LOSE,
            SEQ_WIN,
        } type;
        uint16_t time;
        int activator;
    } sequence;

    struct GameObject {
        enum GameObjectType type;
        ObjectID previous, next;

        int32_t block;
        ObjectID previous_block, next_block;

        fvec2 pos, bbox[2];
        fix16_t depth;

        fix16_t values[MAX_VALUES];
        uint32_t flags;
    } objects[MAX_OBJECTS];
    ObjectID live_objects, next_object;

    ObjectID blockmap[BLOCKMAP_SIZE];
};

struct SaveState {
    struct GameState game;
    struct SoundState audio;
};

struct InterpObject {
    enum GameObjectType type;
    fvec2 from, to;
    fvec2 pos;
};

void start_state(int, int);
void save_state(struct GameState*);
void load_state(const struct GameState*);
uint32_t check_state();
void dump_state();
void tick_state(enum GameInput[MAX_PLAYERS]);
void draw_state();
void draw_state_hud();

void load_object(enum GameObjectType);

bool object_is_alive(ObjectID);
struct GameObject* get_object(ObjectID);
ObjectID create_object(enum GameObjectType, const fvec2);
void move_object(ObjectID, const fvec2);

struct BlockList {
    ObjectID objects[MAX_OBJECTS];
    size_t num_objects;
};

void list_block_at(struct BlockList*, const fvec2[2]);

void kill_object(ObjectID);
void destroy_object(ObjectID);
void draw_object(ObjectID, const char*, GLfloat, const GLubyte[4]);
void play_sound_at_object(struct GameObject*, const char*);

int32_t random();

void interp_start();
void interp_end();
void interp_update(float);
void skip_interp(ObjectID);
