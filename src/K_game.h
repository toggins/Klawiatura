#pragma once

#include <S_fixed.h>

#include "K_net.h" // IWYU pragma: keep

#define TICKRATE 50

#define MAX_PLAYERS 4
#define MAX_MISSILES 2
#define MAX_SINK 6
#define KEVIN_DELAY 50

#define MAX_ACTORS 1000
#define NULLACT ((ActorID)(-1))

#define MAX_VALUES 32

typedef fix16_t ActorValue;
typedef uint32_t ActorFlag;

#define MAX_CELLS 128
#define GRID_SIZE (MAX_CELLS * MAX_CELLS)
#define CELL_SIZE ((fix16_t)(0x01000000))

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH >> 1)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT >> 1)
#define F_SCREEN_WIDTH FfInt(SCREEN_WIDTH)
#define F_SCREEN_HEIGHT FfInt(SCREEN_HEIGHT)
#define F_HALF_SCREEN_WIDTH Fhalf(F_SCREEN_WIDTH)
#define F_HALF_SCREEN_HEIGHT Fhalf(F_SCREEN_HEIGHT)

typedef uint8_t Bool;
typedef int8_t PlayerID;
typedef int16_t ActorID;

typedef uint16_t GameInput;
enum {
    GI_UP = 1 << 0,
    GI_LEFT = 1 << 1,
    GI_DOWN = 1 << 2,
    GI_RIGHT = 1 << 3,
    GI_JUMP = 1 << 4,
    GI_RUN = 1 << 5,
    GI_FIRE = 1 << 6,
};

typedef uint16_t GameFlag;
enum {
    GF_HURRY = 1 << 0,
    GF_END = 1 << 1,
    GF_SCROLL = 1 << 2,
    GF_LAVA = 1 << 3,
    GF_HARDCORE = 1 << 4,
    GF_SPIKES = 1 << 5,
    GF_FUNNY = 1 << 6,
    GF_LOST = 1 << 7,
    GF_KEVIN = 1 << 8,
    GF_AMBUSH = 1 << 9,
    GF_REPLAY = 1 << 10,
    GF_SINGLE = 1 << 11,
    GF_RESTART = 1 << 12,
};

typedef uint8_t GameSequenceType;
enum {
    SEQ_NONE,
    SEQ_LOSE,
    SEQ_WIN,
};

typedef uint16_t GameActorType;
enum {
    ACT_NULL,

    ACT_SOLID,
    ACT_SOLID_TOP,
    ACT_SOLID_SLOPE,
    ACT_PLAYER_SPAWN,
    ACT_PLAYER,
    ACT_PLAYER_EFFECT,
    ACT_PLAYER_DEAD,
    ACT_KEVIN,
    ACT_COIN,
    ACT_COIN_POP,
    ACT_MUSHROOM,
    ACT_MUSHROOM_1UP,
    ACT_MUSHROOM_POISON,
    ACT_FIRE_FLOWER,
    ACT_BEETROOT,
    ACT_LUI,
    ACT_HAMMER_SUIT,
    ACT_STARMAN,
    ACT_POINTS,
    ACT_EXPLODE,
    ACT_MISSILE_FIREBALL,
    ACT_MISSILE_BEETROOT,
    ACT_MISSILE_BEETROOT_SINK,
    ACT_MISSILE_HAMMER,
    ACT_BLOCK_BUMP,
    ACT_ITEM_BLOCK,
    ACT_HIDDEN_BLOCK,
    ACT_BRICK_BLOCK,
    ACT_BRICK_SHARD,
    ACT_BRICK_BLOCK_COIN,
    ACT_CHECKPOINT,
    ACT_GOAL_BAR,
    ACT_GOAL_BAR_FLY,
    ACT_GOAL_MARK,
    ACT_WATER_SPLASH,
    ACT_BUBBLE,
    ACT_DEAD,
    ACT_GOOMBA,
    ACT_GOOMBA_FLAT,
    ACT_KOOPA,
    ACT_KOOPA_SHELL,
    ACT_PARAKOOPA,
    ACT_BUZZY_BEETLE,
    ACT_BUZZY_SHELL,
    ACT_SPINY,
    ACT_LAKITU,
    ACT_SPIKE,
    ACT_PIRANHA_PLANT,
    ACT_BRO,
    ACT_CLOUD_FACE,
    ACT_WARP,
    ACT_MISSILE_SILVER_HAMMER,
    ACT_BRO_LAYER,
    ACT_BILL_BLASTER,
    ACT_BULLET_BILL,
    ACT_CHEEP_CHEEP,
    ACT_BOUNDS,
    ACT_CHEEP_CHEEP_BLUE,
    ACT_CHEEP_CHEEP_SPIKY,
    ACT_BLOOPER,
    ACT_ROTODISC_BALL,
    ACT_ROTODISC,
    ACT_THWOMP,
    ACT_BOO,
    ACT_DRY_BONES,
    ACT_LAVA,
    ACT_PODOBOO_SPAWNER,
    ACT_PODOBOO,
    ACT_SPRING,
    ACT_PSWITCH,
    ACT_PSWITCH_COIN,
    ACT_PSWITCH_BRICK,
    ACT_CLOUD,
    ACT_CLOUD_DARK,
    ACT_BUSH,
    ACT_BUSH_SNOW,
    ACT_AUTOSCROLL,
    ACT_WHEEL_LEFT,
    ACT_WHEEL,
    ACT_WHEEL_RIGHT,
    ACT_SPIKE_CANNON,
    ACT_SPIKE_BALL,
    ACT_SPIKE_BALL_EFFECT,
    ACT_PIRANHA_FIRE,
    ACT_PLATFORM,
    ACT_PLATFORM_TURN,
    ACT_LAVA_SPLASH,
    ACT_PODOBOO_VOLCANO,
    ACT_BOWSER,

    ACT_SIZE,
};

typedef uint8_t PlayerPower;
enum {
    POW_SMALL,
    POW_BIG,
    POW_FIRE,
    POW_BEETROOT,
    POW_LUI,
    POW_HAMMER,
};

typedef uint8_t PlayerFrame;
enum {
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
    PF_DEAD,
};

typedef uint8_t PlatformType;
enum {
    PLAT_NORMAL,
    PLAT_SMALL,
    PLAT_CLOUD,
    PLAT_CASTLE,
    PLAT_CASTLE_LONG,
    PLAT_CASTLE_BUTTONS,
};

struct GameContext {};

struct GamePlayer {
    Bool active;
    GameInput input, last_input;

    ActorID actor;
    fix16_t pos[2], bounds[2];

    int8_t lives;
    uint8_t coins;
    uint32_t score;
    PlayerPower power;

    ActorID missiles[MAX_MISSILES], sink[MAX_SINK];

    struct Kevin {
        ActorID object;
        int8_t delay;
        fix16_t pos[2];

        struct KevinFrame {
            fix16_t pos[2];
            Bool flip;
            PlayerPower power;
            PlayerFrame frame;
        } frames[KEVIN_DELAY];
    } kevin;
};

struct GameSequence {
    GameSequenceType type;
    PlayerID activator;
    uint16_t time;
};

enum BaseActorValues {
    VAL_X_SPEED,
    VAL_Y_SPEED,
    VAL_X_TOUCH,
    VAL_Y_TOUCH,
    VAL_SPROUT,
    VAL_CUSTOM
};

#define CUSTOM_FLAG(idx) (1 << (5 + (idx)))

#define ANY_FLAG(actor, flag) (((actor)->flags & (flag)) > 0)
#define ALL_FLAG(actor, flag) (((actor)->flags & (flag)) == (flag))

#define FLAG_ON(actor, flag) (actor)->flags |= (flag)
#define FLAG_OFF(actor, flag) (actor)->flags &= ~(flag)

#define TOGGLE_FLAG(actor, flag)                                                                                       \
    do {                                                                                                               \
        if (ANY_FLAG(actor, flag))                                                                                     \
            FLAG_OFF(actor, flag);                                                                                     \
        else                                                                                                           \
            FLAG_ON(actor, flag);                                                                                      \
    } while (0)

enum BaseActorFlags {
    FLG_VISIBLE = 1 << 0,
    FLG_DESTROY = 1 << 1,
    FLG_X_FLIP = 1 << 2,
    FLG_Y_FLIP = 1 << 3,
    FLG_FREEZE = 1 << 4,
    FLG_CUSTOM = CUSTOM_FLAG(0),
};

struct GameActor {
    GameActorType type;
    ActorID previous, next;

    int32_t cell;
    ActorID previous_cell, next_cell;

    fix16_t pos[2], box[2][2];
    fix16_t depth;

    ActorValue values[MAX_VALUES];
    ActorFlag flags;
};

struct GameState {
    struct GamePlayer players[MAX_PLAYERS];

    GameFlag flags;
    struct GameSequence sequence;

    char world[8], next[8];
    fix16_t size[2];
    fix16_t bounds[2][2];

    uint64_t time;
    uint32_t seed;

    int32_t clock;
    ActorID spawn, checkpoint, autoscroll;
    fix16_t water, hazard;
    uint16_t pswitch;

    struct GameActor actors[MAX_ACTORS];
    ActorID live_actors, next_actor;
};

struct VideoState {};

struct AudioState {};

struct SaveState {
    struct GameState game;
    struct VideoState video;
    struct AudioState audio;
};

struct GameActorInfo {
    void (*load)();
    void (*create)(struct GameState*, const ActorID, struct GameActor*);
    void (*tick)(struct GameState*, const ActorID, struct GameActor*);
    void (*draw)(const struct GameState*, const ActorID, const struct GameActor*);
    void (*destroy)(struct GameState*, const ActorID, struct GameActor*);
};

struct GameInstance {
    GekkoSession* net;
    struct GameContext context;
    struct SaveState state;
};

struct GameInstance* create_game_instance();
void update_game_instance();
void draw_game_instance();
void destroy_game_instance();
