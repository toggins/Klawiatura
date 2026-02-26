#pragma once

#include "K_audio.h"

typedef Sint8 PlayerID;

#include "K_cmd.h"
#include "K_math.h"
#include "K_net.h" // IWYU pragma: keep

#define GAME_NAME "Klawiatura"
#define GAME_VERSION "1.1.0"

#define MAJOR_LEVEL_VERSION 0
#define MINOR_LEVEL_VERSION 2

#define MAX_PLAYERS 8L
#define DEFAULT_LIVES 4L
#define MAX_MISSILES 2L
#define MAX_SINK 6L
#define KEVIN_DELAY 50L
#define NULLPLAY ((PlayerID)(-1L))

#define MAX_ACTORS 1000L
#define NULLACT ((ActorID)(-1L))
#define NULLCELL ((Sint32)(-1L))

#define MAX_VALUES 32L
#define GAME_STRING_MAX 256L
#define ACTOR_STRING_MAX 16L

typedef Fixed ActorValue;
typedef Uint32 ActorFlag;

#define MAX_CELLS 128L
#define GRID_SIZE (MAX_CELLS * MAX_CELLS)
#define CELL_SIZE ((Fixed)(0x01000000))

#define SCREEN_WIDTH 640L
#define SCREEN_HEIGHT 480L
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH >> 1L)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT >> 1L)
#define F_SCREEN_WIDTH FxFrom(SCREEN_WIDTH)
#define F_SCREEN_HEIGHT FxFrom(SCREEN_HEIGHT)
#define F_HALF_SCREEN_WIDTH Fhalf(F_SCREEN_WIDTH)
#define F_HALF_SCREEN_HEIGHT Fhalf(F_SCREEN_HEIGHT)

#ifndef K_VIDEO_H
#include "K_video.h" // IWYU pragma: keep
#endif

typedef Sint16 ActorID;

typedef Uint8 GameInput;
enum {
	GI_UP = 1 << 0,
	GI_LEFT = 1 << 1,
	GI_DOWN = 1 << 2,
	GI_RIGHT = 1 << 3,
	GI_JUMP = 1 << 4,
	GI_RUN = 1 << 5,
	GI_FIRE = 1 << 6,
	GI_WARP = 1 << 7,
};

typedef Uint16 GameFlag;
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
	GF_FRED = 1 << 13,
	GF_HUB = 1 << 14,

	GF_HELL = GF_KEVIN | GF_FRED,
};

typedef Uint8 GameSequenceType;
enum {
	SEQ_NONE,
	SEQ_LOSE,
	SEQ_WIN,
	SEQ_AMBUSH,
	SEQ_AMBUSH_END,
	SEQ_BOWSER_END,
	SEQ_SECRET,
};

typedef Uint16 GameActorType;
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
	ACT_MISSILE_NAPALM,
	ACT_MISSILE_HAMMER,
	ACT_BLOCK_BUMP,
	ACT_ITEM_BLOCK,
	ACT_HIDDEN_BLOCK,
	ACT_BRICK_BLOCK,
	ACT_BRICK_SHARD,
	ACT_COIN_BLOCK,
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
	ACT_CLOUDS,
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
	ACT_NOTE_BLOCK,
	ACT_SPINY_EGG,
	ACT_PIRANHA_HEAD,
	ACT_ELECTRIC_CORAL,
	ACT_BOWSER_DEAD,
	ACT_BOWSER_LAVA,
	ACT_LAVA_BUBBLE,
	ACT_WAVING_LEVEL,
	ACT_BOSS_BASS,
	ACT_FAKE_CLOUD,
	ACT_FAKE_BLOCK,
	ACT_FRED,
	ACT_LAMP_LIGHT,
	ACT_BUBBLE_TUBE,
	ACT_TUBE_BUBBLE,
	ACT_WATERFALL,
	ACT_LAVAFALL,
	ACT_LAVA_BUBBLE_SPAWNER,
	ACT_PARTY,
	ACT_BENCHMARK,

	ACT_SIZE,
};

typedef Uint8 PlayerPower;
enum {
	POW_SMALL,
	POW_BIG,
	POW_FIRE,
	POW_BEETROOT,
	POW_LUI,
	POW_HAMMER,
};

typedef Uint8 PlayerFrame;
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

typedef Uint8 SolidType;
enum {
	SOL_SOLID = 1 << 0,
	SOL_TOP = 1 << 1,
	SOL_BOTTOM = 1 << 2,
	SOL_ALL = SOL_SOLID | SOL_TOP | SOL_BOTTOM,
};

typedef Uint8 PlatformType;
enum {
	PLAT_NORMAL,
	PLAT_SMALL,
	PLAT_CLOUD,
	PLAT_CASTLE,
	PLAT_CASTLE_BIG,
	PLAT_CASTLE_BUTTON,
};

typedef struct {
	PlayerID num_players;
	GameFlag flags;
	ActorID checkpoint;

	char level[GAME_STRING_MAX];
	struct {
		Sint8 lives;
		Uint8 coins;
		PlayerPower power;
		Uint32 score;
	} players[MAX_PLAYERS];
} GameContext;

typedef struct {
	Sint8 lives;
	Uint32 score;
	char name[CLIENT_STRING_MAX];
} GameWinner;

typedef struct {
	PlayerID id;
	GameInput input, last_input;

	Sint8 lives;
	Uint8 coins;
	PlayerPower power;

	ActorID actor;
	ActorID missiles[MAX_MISSILES], sink[MAX_SINK];
	FVec2 pos;
	FRect bounds;

	Uint32 score;

	struct {
		Sint8 delay;
		ActorID actor;
		FVec2 start;

		struct {
			Bool flip;
			PlayerPower power;
			PlayerFrame frame;
			FVec2 pos;
		} frames[KEVIN_DELAY];
	} kevin;
} GamePlayer;

#define ANY_INPUT(player, inp) (((player)->input & (inp)) != 0L)
#define ALL_INPUT(player, inp) (((player)->input & (inp)) == (inp))
#define ANY_LAST_INPUT(player, inp) (((player)->last_input & (inp)) != 0L)
#define ALL_LAST_INPUT(player, inp) (((player)->last_input & (inp)) == (inp))
#define ANY_PRESSED(player, inp) (ANY_INPUT(player, inp) && !ANY_LAST_INPUT(player, inp))
#define ANY_RELEASED(player, inp) (!ANY_INPUT(player, inp) && ANY_LAST_INPUT(player, inp))

typedef struct {
	GameSequenceType type;
	PlayerID activator;
	Uint16 time;
} GameSequence;

#define VAL_STRING(ident) ident, ident##_END = (ident) + (ACTOR_STRING_MAX / sizeof(ActorValue))
enum BaseActorValues {
	VAL_X_SPEED,
	VAL_Y_SPEED,
	VAL_X_TOUCH,
	VAL_Y_TOUCH,
	VAL_SPROUT,
	VAL_CUSTOM
};

#define VAL(actor, val) ((actor)->values[VAL_##val])
#define VAL_TICK(actor, val)                                                                                           \
	do {                                                                                                           \
		if (VAL(actor, val) > 0L)                                                                              \
			--VAL(actor, val);                                                                             \
	} while (0)

#define ANY_FLAG(actor, flag) (((actor)->flags & (flag)) != 0L)
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

#define POS_ADD(actor, _x, _y) ((FVec2){(actor)->pos.x + (_x), (actor)->pos.y + (_y)})
#define POS_SPEED(actor)                                                                                               \
	((FVec2){(actor)->pos.x + (actor)->values[VAL_X_SPEED], (actor)->pos.y + (actor)->values[VAL_Y_SPEED]})

#define HITBOX(actor)                                                                                                  \
	((FRect){                                                                                                      \
		{(actor)->pos.x + (actor)->box.start.x, (actor)->pos.y + (actor)->box.start.y},                        \
		{(actor)->pos.x + (actor)->box.end.x,   (actor)->pos.y + (actor)->box.end.y  },                            \
	})
#define HITBOX_ADD(actor, _x, _y)                                                                                      \
	((FRect){                                                                                                      \
		{(actor)->pos.x + (actor)->box.start.x + (_x), (actor)->pos.y + (actor)->box.start.y + (_y)},          \
		{(actor)->pos.x + (actor)->box.end.x + (_x),   (actor)->pos.y + (actor)->box.end.y + (_y)  },              \
	})
#define HITBOX_LEFT(actor)                                                                                             \
	((FRect){                                                                                                      \
		{(actor)->pos.x + (actor)->box.start.x,         (actor)->pos.y + (actor)->box.start.y},                        \
		{(actor)->pos.x + (actor)->box.start.x + FxOne, (actor)->pos.y + (actor)->box.end.y  },                  \
	})
#define HITBOX_RIGHT(actor)                                                                                            \
	((FRect){                                                                                                      \
		{(actor)->pos.x + (actor)->box.end.x - FxOne, (actor)->pos.y + (actor)->box.start.y},                  \
		{(actor)->pos.x + (actor)->box.end.x,         (actor)->pos.y + (actor)->box.end.y  },                            \
	})

typedef struct {
	ActorID id;
	GameActorType type;
	ActorID previous, next;

	ActorID previous_cell, next_cell;
	Sint32 cell;

	FVec2 pos;
	FRect box;
	Fixed depth;

	ActorFlag flags;
	ActorValue values[MAX_VALUES];
} GameActor;

typedef struct {
	GameFlag flags;
	GameSequence sequence;

	GamePlayer players[MAX_PLAYERS];

	ActorID spawn, checkpoint, autoscroll, wave;
	Uint16 pswitch;

	FVec2 size;
	FRect bounds;

	Fixed water, hazard;
	Sint32 clock;

	Sint32 seed;
	Uint64 time;

	ActorID live_actors, next_actor;
	GameActor actors[MAX_ACTORS];
	ActorID grid[GRID_SIZE];

	Sint8 next[GAME_STRING_MAX];
} GameState;

typedef struct {
	GameState game;
	AudioState audio;
} SaveState;

typedef struct {
	GameActorType type;
	FVec2 from, pos;
} InterpActor;

typedef struct {
	InterpActor actors[MAX_ACTORS];
} InterpState;

SolidType always_solid(const GameActor*), always_top(const GameActor*), always_bottom(const GameActor*);

typedef struct {
	SolidType (*is_solid)(const GameActor*);
	void (*load)(), (*load_special)(const GameActor*);
	void (*create)(GameActor*);
	void (*pre_tick)(GameActor*), (*tick)(GameActor*), (*post_tick)(GameActor*);
	void (*draw)(const GameActor*), (*draw_dead)(const GameActor*), (*draw_hud)(const GameActor*);
	void (*cleanup)(GameActor*);
	void (*collide)(GameActor*, GameActor*);
	void (*on_top)(GameActor*, GameActor*), (*on_bottom)(GameActor*, GameActor*);
	void (*on_left)(GameActor*, GameActor*), (*on_right)(GameActor*, GameActor*);
	PlayerID (*owner)(const GameActor*);
} GameActorTable;

extern GameState game_state;

GameContext* init_game_context();

void start_game(), nuke_game();
Bool game_exists();
Bool update_game();
void draw_game();

void hud_message(const char*);

// ====
// CHAT
// ====

void push_chat_message(int, const char*, int);

// =======
// PLAYERS
// =======

GamePlayer *get_player(PlayerID), *get_owner(const GameActor*);
PlayerID get_owner_id(const GameActor*);
GameActor *respawn_player(GamePlayer*), *nearest_pawn(const FVec2);

PlayerID localplayer(), viewplayer(), numplayers();
void set_view_player(GamePlayer*);

const char* get_player_name(PlayerID);

// ======
// ACTORS
// ======

void load_actor(GameActorType);
GameActor* create_actor(GameActorType, const FVec2);
void replace_actors(GameActorType, GameActorType);

GameActor* get_actor(ActorID);
void move_actor(GameActor*, const FVec2);

Bool below_level(const GameActor*);
Bool in_any_view(const GameActor*, Fixed, Bool);
Bool in_player_view(const GameActor*, GamePlayer*, Fixed, Bool);

typedef struct {
	GameActor* actors[MAX_ACTORS];
	ActorID num_actors;
} CellList;
void list_cell_at(CellList*, const FRect);
void collide_actor(GameActor*);
Bool touching_solid(const FRect, SolidType);
void displace_actor(GameActor*, Fixed, Bool);
void displace_actor_soft(GameActor*);

void encode_actor_string(GameActor*, ActorValue, const Sint8[ACTOR_STRING_MAX]);
void decode_actor_string(const GameActor*, ActorValue, Sint8[ACTOR_STRING_MAX]);

void draw_actor(const GameActor*, const char*, float, const Uint8[4]);
void draw_actor_offset(const GameActor*, const char*, const float[3], float, const Uint8[4]);
void draw_actor_no_jitter(const GameActor*, const char*, float, const Uint8[4]);
void draw_dead(const GameActor*);
void quake_at_actor(const GameActor*, float);

// ====
// MATH
// ====

Sint32 rng(Sint32);

// =============
// INTERPOLATION
// =============

const InterpActor* get_interp(const GameActor*);
void skip_interp(const GameActor*);
void align_interp(const GameActor*, const GameActor*);
