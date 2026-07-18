#pragma once

#include "K_cmake.h" // IWYU pragma: export
#include "K_math.h"
#include "K_memory.h" // IWYU pragma: export

typedef Sint8 PlayerID;

#define MAX_PLAYERS 8
#define NULL_PLAYER ((PlayerID)(-1))

#define DEFAULT_LIVES 4
#define MAX_PROJECTILES 2
#define MAX_SINKING_PROJECTILES 6

#define MAX_ACTORS 1000
#define NULL_ACTOR ((ActorID)(-1))

#define MAX_VALUES 32

#define MAX_CELLS 128
#define GRID_SIZE (MAX_CELLS * MAX_CELLS)
#define CELL_SIZE Int2Fx(256)
#define NULL_CELL ((Sint32)(-1))

typedef Fixed ActorValue;
typedef Uint32 ActorFlag;

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH >> 1)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT >> 1)
#define F_SCREEN_WIDTH Int2Fx(SCREEN_WIDTH)
#define F_SCREEN_HEIGHT Int2Fx(SCREEN_HEIGHT)
#define F_HALF_SCREEN_WIDTH Fhalf(F_SCREEN_WIDTH)
#define F_HALF_SCREEN_HEIGHT Fhalf(F_SCREEN_HEIGHT)

#define F_SCREEN ((FVec2){F_SCREEN_WIDTH, F_SCREEN_HEIGHT})
#define F_HALF_SCREEN ((FVec2){F_HALF_SCREEN_WIDTH, F_HALF_SCREEN_HEIGHT})

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
};

typedef Uint16 GameFlags;
enum {
    GF_END = 1 << 0,       // Game session should end
    GF_RESTARTED = 1 << 1, // Level was restarted

    GF_HURRY = 1 << 2, // Time <= 100

    GF_HARDCORE = 1 << 3, // Hardcore World level
    GF_FUNNY = 1 << 4,    // Funny Tanks? level
    GF_LOST = 1 << 5,     // Lost Map level
    GF_AMBUSH = 1 << 6,   // Ambush level
};

typedef Uint8 GameSequenceType;
enum {
    GS_NONE,
    GS_LOSE,
    GS_WIN,
    GS_AMBUSH,
    GS_AMBUSH_END,
    GS_BOWSER_END,
    GS_SECRET,
};

typedef Uint8 ActorType;
enum {
    ACT_NULL,

    ACT_SOLID,
    ACT_SOLID_TOP,
    ACT_SOLID_SLOPE,
    ACT_PLAYER_SPAWN,
    ACT_PLAYER,
    ACT_PLAYER_EFFECT,
    ACT_PLAYER_DEAD,
    ACT_CHECKPOINT,
    ACT_WATER,

    ACT_DUMMY = 254,

    ACT_SIZE,
};

typedef Uint8 PlayerCharacter;
enum {
    CHR_MARIO,
    CHR_SIZE,
};

typedef Uint8 PlayerPowerup;
enum {
    POW_NONE,
    POW_SUPER_MUSHROOM,
    POW_FIRE_FLOWER,
    POW_BEETROOT,
    POW_GREEN_LUI,
    POW_SIZE,
};

#include "K_worlds.h"

typedef Uint8 PlayerFrame;
enum {
    PF_IDLE,
    PF_WALK1,
    PF_WALK2,
    PF_WALK3,
    PF_JUMP,
    PF_FALL,
    PF_DUCK,
    PF_FIRE1,
    PF_FIRE2,
    PF_SWIM1,
    PF_SWIM2,
    PF_SWIM3,
    PF_SWIM4,
    PF_SWIM5,
    PF_SWIM6,
    PF_SWIM7,
    PF_SWIM8,
    PF_GROW1,
    PF_GROW2,
    PF_GROW3,
    PF_GROW4,
    PF_DEAD,
    PF_WIN,
    PF_SIZE,
};

typedef Uint8 PlayerVoice;
enum {
    PV_READY,
    PV_CHECKPOINT1,
    PV_CHECKPOINT2,
    PV_CHECKPOINT3,
    PV_PANIC,
    PV_SIZE,
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
    const char *name, *cursor, *sprites[POW_SIZE][PF_SIZE], *voices[PV_SIZE];
} GameCharacter;

typedef struct {
    Bool xscroll;
    Sint8 lives;
    Uint8 coins;
    PlayerCharacter character;
    PlayerPowerup powerup;
    Uint32 score;
} GamePlayerContext;

typedef struct {
    PlayerID num_players;
    GameFlags flags;
    ActorID checkpoint;
    TinyHash level;

    GamePlayerContext players[MAX_PLAYERS];
} GameContext;

typedef struct {
    PlayerID id;
    GameInput input, last_input;

    Sint8 lives;
    Uint8 coins;
    PlayerPowerup powerup;

    Uint8 track;
    ActorID actor;
    ActorID projectiles[MAX_PROJECTILES], sinking_projectiles[MAX_SINKING_PROJECTILES];
    FVec2 pos;
    FRect bounds;

    Uint32 score;
} GamePlayer;

#define ANY_INPUT(player, inp) (((player)->input & (inp)) != 0)
#define ALL_INPUT(player, inp) (((player)->input & (inp)) == (inp))
#define ANY_LAST_INPUT(player, inp) (((player)->last_input & (inp)) != 0)
#define ALL_LAST_INPUT(player, inp) (((player)->last_input & (inp)) == (inp))
#define ANY_PRESSED(player, inp) (ANY_INPUT(player, inp) && !ANY_LAST_INPUT(player, inp))
#define ALL_PRESSED(player, inp) (ALL_INPUT(player, inp) && !ALL_LAST_INPUT(player, inp))
#define ANY_RELEASED(player, inp) (!ANY_INPUT(player, inp) && ANY_LAST_INPUT(player, inp))
#define ALL_RELEASED(player, inp) (!ALL_INPUT(player, inp) && ALL_LAST_INPUT(player, inp))

typedef struct {
    GameSequenceType type;
    PlayerID activator;
    Uint16 time;
} GameSequence;

enum {
    VAL_X_SPEED,
    VAL_Y_SPEED,
    VAL_X_TOUCH,
    VAL_Y_TOUCH,
    VAL_SPROUT,
    VAL_CUSTOM
};

#define VAL(actor, val) ((actor)->values[VAL_##val])
#define VAL_TICK(actor, val)                                                                                           \
    do {                                                                                                               \
        if (VAL(actor, val) > 0)                                                                                       \
            --VAL(actor, val);                                                                                         \
    } while (FALSE)

#define ANY_FLAG(actor, flag) (((actor)->flags & (flag)) != 0)
#define ALL_FLAG(actor, flag) (((actor)->flags & (flag)) == (flag))

#define FLAG_ON(actor, flag) ((actor)->flags |= (flag))
#define FLAG_OFF(actor, flag) ((actor)->flags &= ~(flag))

#define TOGGLE_FLAG(actor, flag) (ANY_FLAG(actor, flag) ? FLAG_OFF(actor, flag) : FLAG_ON(actor, flag))

typedef Uint32 ActorFlags;
enum {
    FLG_VISIBLE = 1 << 0,
    FLG_DESTROY = 1 << 1,
    FLG_X_FLIP = 1 << 2,
    FLG_Y_FLIP = 1 << 3,
    FLG_FREEZE = 1 << 4,
#define CUSTOM_FLAG(idx) (1 << (5 + (idx)))
};

typedef struct {
    ActorID id;
    ActorType type;
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
    GameFlags flags;
    GameSequence sequence;

    GamePlayer players[MAX_PLAYERS];

    ActorID spawn, checkpoint, water;

    Sint32 clock;

    Uint64 seed;
    Uint64 time;

    ActorID live_actors, next_actor;
    GameActor actors[MAX_ACTORS];
    ActorID grid[GRID_SIZE];
} GameState;

SolidType always_solid(const GameActor*), always_top(const GameActor*), always_bottom(const GameActor*);

typedef struct {
    SolidType (*is_solid)(const GameActor*);
    void (*load)(), (*load_special)(const GameActor*);
    void (*create)(GameActor*);
    void (*pre_tick)(GameActor*), (*tick)(GameActor*), (*post_tick)(GameActor*);
    void (*draw)(const GameActor*), (*draw_dead)(const GameActor*), (*draw_hud)(const GameActor*);
    void (*cleanup)(GameActor*);
    void (*collide)(GameActor*, GameActor*);
    void (*on_left)(GameActor*, GameActor*), (*on_right)(GameActor*, GameActor*);
    void (*on_top)(GameActor*, GameActor*), (*on_bottom)(GameActor*, GameActor*);
    PlayerID (*owner)(const GameActor*);
} GameActorTable;

typedef Uint8 GameWarpID;
#define MAX_GAME_WARPS 4

typedef Uint8 GameStringID;
enum {
    GSTR_LABEL,
    GSTR_TRACK_START,
    GSTR_TRACK1 = GSTR_TRACK_START,
    GSTR_TRACK2,
    GSTR_TRACK3,
    GSTR_TRACK4,
    GSTR_TRACK_END = GSTR_TRACK4,
    GSTR_SECRET_START,
    GSTR_SECRET1 = GSTR_SECRET_START,
    GSTR_SECRET2,
    GSTR_SECRET3,
    GSTR_SECRET4,
    GSTR_SECRET_END = GSTR_SECRET4,
    GSTR_SIZE,
};

typedef struct {
    FVec2 size;
    FRect bounds;

    TinyHash warps[MAX_GAME_WARPS];
    const char* strings[GSTR_SIZE];
} LevelInfo;

void game_init();

Uint32 get_game_hash();

const GameCharacter* get_character(PlayerCharacter);
const char *get_character_name(PlayerCharacter), *get_character_cursor(PlayerCharacter),
    *get_character_sprite(PlayerCharacter, PlayerPowerup, PlayerFrame),
    *get_character_voice(PlayerCharacter, PlayerVoice);

const char* get_powerup_name(PlayerPowerup);
Sint8 get_powerup_cost(PlayerPowerup);

GameContext empty_game_context(), init_game_context(const struct WorldContext*, TinyHash);

void jump_to_game(const GameContext*, Bool);
void start_game(const GameContext*), nuke_game();
void poll_game();
float frames_ahead();
void tick_game(), draw_game();

const GameContext* gamecontext();
const LevelInfo* levelinfo();
GameState* gamestate();

PlayerID localplayer(), viewplayer();
void set_view_player(const GamePlayer*);
GamePlayer *get_player(PlayerID), *get_owner(const GameActor*);
GameActor *respawn_player(GamePlayer*), *nearest_player_actor(const FVec2);

void load_actor(ActorType);
GameActor *create_actor(ActorType, const FVec2), *get_actor(ActorID);
void replace_actors(ActorType, ActorType);

void move_actor(GameActor*, const FVec2);

Bool below_level(const GameActor*);
Bool in_any_view(const GameActor*, Fixed, Bool);
Bool in_player_view(const GameActor*, const GamePlayer*, Fixed, Bool);

void collide_actor(GameActor*);
Bool touching_solid(const FRect, SolidType);
void displace_actor(GameActor*, Fixed, Bool);
void displace_actor_soft(GameActor*);

void draw_actor(const GameActor*, const char*, float, const Uint8[4], Bool);
void draw_actor_offset(const GameActor*, const char*, const float[3], float, const Uint8[4], Bool);
void draw_dead(const GameActor*);
void quake_at_actor(const GameActor*, float);

Sint32 rng(Sint32);
