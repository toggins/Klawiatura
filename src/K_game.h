#pragma once

#include <S_fixed.h>

#include "K_net.h" // IWYU pragma: keep

#define TICKRATE 50

#define MAX_PLAYERS 4
#define MAX_MISSILES 2
#define MAX_SINK 6
#define KEVIN_DELAY 50
#define NULLPLAY ((PlayerID)(-1))

#define MAX_ACTORS 1000
#define NULLACT ((ActorID)(-1))

typedef fix16_t fvec2[2];
typedef fvec2 frect[2];

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

typedef uint8_t GameInput;
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

typedef struct {
} GameContext;

typedef struct {
	PlayerID id;
	GameInput input, last_input;

	int8_t lives;
	uint8_t coins;
	PlayerPower power;

	ActorID actor;
	ActorID missiles[MAX_MISSILES], sink[MAX_SINK];
	fvec2 pos;
	frect bounds;

	uint32_t score;

	struct {
		int8_t delay;
		ActorID object;
		fvec2 start;

		struct {
			Bool flip;
			PlayerPower power;
			PlayerFrame frame;
			fvec2 pos;
		} frames[KEVIN_DELAY];
	} kevin;
} GamePlayer;

typedef struct {
	GameSequenceType type;
	PlayerID activator;
	uint16_t time;
} GameSequence;

enum BaseActorValues {
	VAL_X_SPEED,
	VAL_Y_SPEED,
	VAL_X_TOUCH,
	VAL_Y_TOUCH,
	VAL_SPROUT,
	VAL_CUSTOM
};

#define ANY_FLAG(actor, flag) (((actor)->flags & (flag)) != 0)
#define ALL_FLAG(actor, flag) (((actor)->flags & (flag)) == (flag))

#define FLAG_ON(actor, flag) ((actor)->flags |= (flag))
#define FLAG_OFF(actor, flag) ((actor)->flags &= ~(flag))

#define TOGGLE_FLAG(actor, flag) (ANY_FLAG(actor, flag) ? FLAG_OFF(actor, flag) : FLAG_ON(actor, flag))

enum BaseActorFlags {
	FLG_VISIBLE = 1 << 0,
	FLG_DESTROY = 1 << 1,
	FLG_X_FLIP = 1 << 2,
	FLG_Y_FLIP = 1 << 3,
	FLG_FREEZE = 1 << 4,
#define CUSTOM_FLAG(idx) (1 << (5 + (idx)))
};

typedef struct {
	ActorID id;
	GameActorType type;
	ActorID previous, next;

	ActorID previous_cell, next_cell;
	int32_t cell;

	fvec2 pos;
	frect box;
	fix16_t depth;

	ActorFlag flags;
	ActorValue values[MAX_VALUES];
} GameActor;

typedef struct {
	GameFlag flags;
	GameSequence sequence;

	GamePlayer players[MAX_PLAYERS];

	ActorID spawn, checkpoint, autoscroll;
	uint16_t pswitch;

	fvec2 size;
	frect bounds;

	fix16_t water, hazard;
	int32_t clock;

	int32_t seed;
	uint64_t time;

	ActorID live_actors, next_actor;
	GameActor actors[MAX_ACTORS];
	ActorID grid[GRID_SIZE];

	char world[256], next[256];
} GameState;

typedef struct {
} VideoState;

typedef struct {
} AudioState;

typedef struct {
	GameState game;
	VideoState video;
	AudioState audio;
} SaveState;

typedef struct {
	void (*load)();
	void (*create)(GameState*, GameActor*);
	void (*tick)(GameState*, GameActor*);
	void (*draw)(const GameState*, const GameActor*);
	void (*destroy)(GameState*, GameActor*);
} GameActorTable;

typedef struct {
	GekkoSession* net;
	GameContext context;
	SaveState state;
} GameInstance;

GameInstance* create_game_instance();
void update_game_instance();
void draw_game_instance();
void destroy_game_instance();

// Players
GamePlayer* get_player(GameState*, PlayerID);

// Actors
GameActor* get_actor(GameState*, ActorID);

// Math
int32_t rng(GameState*, int32_t);

fix16_t point_distance(const fvec2, const fvec2);
fix16_t point_angle(const fvec2, const fvec2);
