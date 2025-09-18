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

#define BASE_VALUES                                                                                                    \
    ActorValue x_speed;                                                                                                \
    ActorValue y_speed;                                                                                                \
    ActorValue x_touch;                                                                                                \
    ActorValue y_touch;                                                                                                \
    ActorValue sprout;

#define BASE_FLAGS                                                                                                     \
    ActorFlag visible : 1;                                                                                             \
    ActorFlag destroy : 1;                                                                                             \
    ActorFlag flip_x : 1;                                                                                              \
    ActorFlag flip_y : 1;                                                                                              \
    ActorFlag freeze : 1;

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

enum GameInput {
    GI_UP = 1 << 0,
    GI_LEFT = 1 << 1,
    GI_DOWN = 1 << 2,
    GI_RIGHT = 1 << 3,
    GI_JUMP = 1 << 4,
    GI_RUN = 1 << 5,
    GI_FIRE = 1 << 6,
};
typedef uint16_t GameInput;

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
    GF_AMBUSH = 1 << 9,
    GF_REPLAY = 1 << 10,
    GF_SINGLE = 1 << 11,
    GF_RESTART = 1 << 12,
};
typedef uint16_t GameFlags;

enum GameSequenceTypes {
    SEQ_NONE,
    SEQ_LOSE,
    SEQ_WIN,
};
typedef uint8_t GameSequenceTypes;

enum GameActorTypes {
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
typedef uint16_t GameActorTypes;

enum PlayerPowers {
    POW_SMALL,
    POW_BIG,
    POW_FIRE,
    POW_BEETROOT,
    POW_LUI,
    POW_HAMMER,
};
typedef uint8_t PlayerPowers;

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
    PF_DEAD,
};
typedef uint8_t PlayerFrames;

enum PlatformTypes {
    PLAT_NORMAL,
    PLAT_SMALL,
    PLAT_CLOUD,
    PLAT_CASTLE,
    PLAT_CASTLE_LONG,
    PLAT_CASTLE_BUTTONS,
};
typedef uint8_t PlatformTypes;

struct GameContext {};

struct GamePlayer {
    Bool active;
    GameInput input, last_input;

    ActorID actor;
    fix16_t pos[2], bounds[2];

    int8_t lives;
    uint8_t coins;
    uint32_t score;
    PlayerPowers power;

    ActorID missiles[MAX_MISSILES], sink[MAX_SINK];

    struct Kevin {
        ActorID object;
        int8_t delay;
        fix16_t pos[2];

        struct KevinFrame {
            fix16_t pos[2];
            Bool flip;
            PlayerPowers power;
            PlayerFrames frame;
        } frames[KEVIN_DELAY];
    } kevin;
};

struct GameSequence {
    GameSequenceTypes type;
    PlayerID activator;
    uint16_t time;
};

struct GameActor {
    GameActorTypes type;
    ActorID previous, next;

    int32_t cell;
    ActorID previous_cell, next_cell;

    fix16_t pos[2], box[2][2];
    fix16_t depth;

    union GameActorValues {
        ActorValue raw[MAX_VALUES];

        struct BaseActorValues {
            BASE_VALUES;
        } base;

        struct PropActorValues {
            BASE_VALUES;
            ActorValue frame;
            ActorValue angle;
            ActorValue alpha;
        } prop, effect;

        struct PlayerActorValues {
            BASE_VALUES;
            ActorValue player;
            ActorValue frame;
            ActorValue ground;
            ActorValue spring;
            ActorValue power;
            ActorValue flash;
            ActorValue star, star_combo;
            ActorValue fire;
            ActorValue warp, warp_state;
            ActorValue platform;
        } player;

        struct PlayerEffectActorValues {
            BASE_VALUES;
            ActorValue player;
            ActorValue power;
            ActorValue frame;
            ActorValue alpha;
        } player_effect;

        struct KevinActorValues {
            BASE_VALUES;
            ActorValue player;
        } kevin;

        struct MissileActorValues {
            BASE_VALUES;
            ActorValue owner;
            ActorValue angle;
            ActorValue hits, hit;
            ActorValue bubble;
            ActorValue alpha;
        } missile;

        struct PointsActorValues {
            BASE_VALUES;
            ActorValue points;
            ActorValue player;
            ActorValue time;
        } points;

        struct BlockActorValues {
            BASE_VALUES;
            ActorValue bump;
            ActorValue owner;
            ActorValue item;
            ActorValue time;
        } block;

        struct CoinActorValues {
            BASE_VALUES;
            ActorValue owner;
            ActorValue frame;
        } coin;

        struct RotodiscActorValues {
            BASE_VALUES;
            ActorValue child, parent;
            ActorValue length;
            ActorValue angle;
            ActorValue speed;
        } rotodisc;

        struct BoundsActorValues {
            BASE_VALUES;
            ActorValue bounds_x1, bounds_y1;
            ActorValue bounds_x2, bounds_y2;
        } bounds, checkpoint;

        struct GoalActorValues {
            BASE_VALUES;
            ActorValue y;
            ActorValue angle;
        } goal;

        struct TimeActorValues {
            BASE_VALUES;
            ActorValue time;
        } pswitch, cannon;

        struct ThwompActorValues {
            BASE_VALUES;
            ActorValue x, y;
            ActorValue state;
            ActorValue frame;
        } thwomp;

        struct DeadActorValues {
            BASE_VALUES;
            ActorValue type;
        } dead;

        struct EnemyActorValues {
            BASE_VALUES;
            ActorValue turn;
            ActorValue mayday;
            ActorValue flat;
        } enemy;

        struct ParakoopaActorValues {
            BASE_VALUES;
            ActorValue x, y;
            ActorValue angle;
        } parakoopa;

        struct ShellActorValues {
            BASE_VALUES;
            ActorValue owner;
            ActorValue hit;
            ActorValue combo;
            ActorValue frame;
        } shell;

        struct PiranhaActorValues {
            BASE_VALUES;
            ActorValue move;
            ActorValue wait;
            ActorValue fire;
        } piranha;

        struct BroActorValues {
            BASE_VALUES;
            ActorValue missile;
            ActorValue move;
            ActorValue unk2, unk3, unk4, unk5;
            ActorValue jump_state, jump;
            ActorValue down;
            ActorValue throw_state, throw;
        } bro;

        struct WarpActorValues {
            BASE_VALUES;
            ActorValue angle;
            ActorValue x, y;
            ActorValue bounds_x1, bounds_y1;
            ActorValue bounds_x2, bounds_y2;
            ActorValue angle_out;
            ActorValue str1, str2;
        } warp;

        struct PlatformActorValues {
            BASE_VALUES;
            ActorValue type;
            ActorValue x, y;
            ActorValue respawn;
            ActorValue frame;
        } platform;

        struct PodobooActorValues {
            BASE_VALUES;
            ActorValue child, parent;
            ActorValue spawn;
        } podoboo;
    } values;

    union GameActorFlags {
        ActorFlag raw;

        struct BaseActorFlags {
            BASE_FLAGS;
        } base;

        struct PropActorFlags {
            BASE_FLAGS;
            ActorFlag extra : 1;
        } prop;

        struct PlayerActorFlags {
            BASE_FLAGS;
            ActorFlag duck : 1;
            ActorFlag jump : 1;
            ActorFlag swim : 1;
            ActorFlag ascend : 1;
            ActorFlag descend : 1;
            ActorFlag respawn : 1;
            ActorFlag stomp : 1;
            ActorFlag warp_out : 1;
            ActorFlag dead : 1;
        } player;

        struct PowerupActorFlags {
            BASE_FLAGS;
            ActorFlag full : 1;
        } powerup;

        struct BlockActorFlags {
            BASE_FLAGS;
            ActorFlag empty : 1;
            ActorFlag gray : 1;
            ActorFlag once : 1;
        } block;

        struct CoinActorFlags {
            BASE_FLAGS;
            ActorFlag start : 1;
            ActorFlag spark : 1;
        } coin;

        struct RotodiscActorFlags {
            BASE_FLAGS;
            ActorFlag start : 1;
            ActorFlag flower : 1;
            ActorFlag flower2 : 1;
        } rotodisc;

        struct GoalActorFlags {
            BASE_FLAGS;
            ActorFlag start : 1;
        } goal;

        struct PSwitchActorFlags {
            BASE_FLAGS;
            ActorFlag once : 1;
        } pswitch;

        struct ScrollActorFlags {
            BASE_FLAGS;
            ActorFlag tanks : 1;
        } scroll;

        struct ThwompActorFlags {
            BASE_FLAGS;
            ActorFlag start : 1;
            ActorFlag laugh : 1;
        } thwomp;

        struct BlasterActorFlags {
            BASE_FLAGS;
            ActorFlag blocked : 1;
        } blaster;

        struct BulletActorFlags {
            BASE_FLAGS;
            ActorFlag dead : 1;
        } bullet;

        struct EnemyActorFlags {
            BASE_FLAGS;
            ActorFlag special : 1;
            ActorFlag active : 1;
        } enemy;

        struct PiranhaActorFlags {
            BASE_FLAGS;
            ActorFlag fire : 1;
            ActorFlag red : 1;
            ActorFlag start : 1;
            ActorFlag blocked : 1;
            ActorFlag out : 1;
            ActorFlag in : 1;
            ActorFlag fired : 1;
        } piranha;

        struct BroActorFlags {
            BASE_FLAGS;
            ActorFlag active : 1;
            ActorFlag unk10 : 1;
            ActorFlag unk11 : 1;
            ActorFlag jump : 1;
            ActorFlag unk30 : 1;
            ActorFlag unk31 : 1;
            ActorFlag bottom : 1;
            ActorFlag top : 1;
        } bro;

        struct BroLayerActorFlags {
            BASE_FLAGS;
            ActorFlag top : 1;
        } bro_layer;

        struct WarpActorFlags {
            BASE_FLAGS;
            ActorFlag goal : 1;
            ActorFlag secret : 1;
        } warp;

        struct PlatformActorFlags {
            BASE_FLAGS;
            ActorFlag start : 1;
            ActorFlag carrying : 1;
            ActorFlag fall : 1;
            ActorFlag down : 1;
            ActorFlag falling : 1;
            ActorFlag moved : 1;
        } platform;

        struct PlatformTurnActorFlags {
            BASE_FLAGS;
            ActorFlag add : 1;
        } platform_turn;
    } flags;
};

struct GameState {
    struct GamePlayer players[MAX_PLAYERS];

    GameFlags flags;
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
