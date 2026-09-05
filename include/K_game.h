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
#define GI_UP (GameInput)(1U << 0)
#define GI_LEFT (GameInput)(1U << 1)
#define GI_DOWN (GameInput)(1U << 2)
#define GI_RIGHT (GameInput)(1U << 3)
#define GI_JUMP (GameInput)(1U << 4)
#define GI_RUN (GameInput)(1U << 5)
#define GI_FIRE (GameInput)(1U << 6)

typedef Uint16 GameFlags;
#define GF_END (GameFlags)(1U << 0)         // Game session should end
#define GF_RESTARTED (GameFlags)(1U << 1)   // Level was restarted
#define GF_HURRY (GameFlags)(1U << 2)       // Time <= 100
#define GF_HARDCORE (GameFlags)(1U << 3)    // Hardcore Level
#define GF_LOST_MAP (GameFlags)(1U << 4)    // Lost Map Level
#define GF_FUNNY_TANKS (GameFlags)(1U << 5) // Funny Tanks? Level
#define GF_1UP (GameFlags)(1U << 6)         // Free 1UPs

typedef Uint8 GameSequenceType;
enum {
    GS_NONE,
    GS_LOSE,
    GS_WIN,
    GS_WARP,
    GS_BOWSER_END,
    GS_AMBUSH,
};

typedef Uint8 ActorType;
enum {
    ACT_NULL,

    ACT_PLAYER_SPAWN,
    ACT_PLAYER,
    ACT_PLAYER_EFFECT,
    ACT_PLAYER_DEAD,
    ACT_POINTS,
    ACT_WARP,
    ACT_CHECKPOINT,
    ACT_CHECKPOINT_EFFECT,
    ACT_GOAL_BAR,
    ACT_GOAL_MARK,
    ACT_PLATFORM,
    ACT_PLATFORM_TURN,
    ACT_WATER,
    ACT_WATER_TRIGGER,
    ACT_AUTOSCROLL,
    ACT_BOUNDS,
    ACT_BUSH,
    ACT_CLOUD,
    ACT_CLOUDS,
    ACT_COIN,
    ACT_COIN_POP,
    ACT_WATER_SPLASH,
    ACT_BUBBLE,
    ACT_BLOCK,
    ACT_BLOCK_BUMP,
    ACT_BRICK_SHARD,
    ACT_SUPER_MUSHROOM,
    ACT_FIRE_FLOWER,
    ACT_STARMAN,
    ACT_1UP_MUSHROOM,
    ACT_POISON_MUSHROOM,
    ACT_GREEN_LUI,
    ACT_BEETROOT,
    ACT_FIREBALL_PROJECTILE,
    ACT_BEETROOT_PROJECTILE,
    ACT_EXPLODE,
    ACT_GOOMBA,
    ACT_DEAD,
    ACT_KOOPA,
    ACT_KOOPA_SHELL,
    ACT_PIRANHA_PLANT,
    ACT_LAMP_LIGHT,
    ACT_TUBE_BUBBLES,
    ACT_TUBE_BUBBLE,
    ACT_WATERFALL,
    ACT_GOOMBA_PARTY,
    ACT_LAVA,
    ACT_LAVAFALL,
    ACT_LAVA_BUBBLER,
    ACT_LAVA_BUBBLE,
    ACT_ROTODISC_BALL,
    ACT_ROTODISC,
    ACT_BOWSER,
    ACT_BOWSER_DEAD,
    ACT_BOWSER_FIRE_PROJECTILE,
    ACT_LAVA_WAVER,
    ACT_BOWSER_EFFECT,
    ACT_SINK,
    ACT_SINK_BUBBLE,
    ACT_CHEEP_SPAWNER,
    ACT_CHEEP,
    ACT_CHEEP_BLUE,
    ACT_SPRING,
    ACT_BRO,
    ACT_BRO_LAYER,
    ACT_HAMMER_PROJECTILE,
    ACT_SILVER_HAMMER_PROJECTILE,
    ACT_CLOUD_FACE,

    ACT_SIZE,
};

typedef Uint8 PlayerCharacter;
enum {
    CHR_MARIO,
    CHR_LUIGI,
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
    PF_WALK,
    PF_WALK1 = PF_WALK,
    PF_WALK2,
    PF_WALK3,
    PF_JUMP,
    PF_FALL,
    PF_DUCK,
    PF_FIRE,
    PF_FIRE1 = PF_FIRE,
    PF_FIRE2,
    PF_SWIM,
    PF_SWIM1 = PF_SWIM,
    PF_SWIM2,
    PF_SWIM3,
    PF_SWIM4,
    PF_SWIM5,
    PF_SWIM6,
    PF_SWIM7,
    PF_SWIM8,
    PF_GROW,
    PF_GROW1 = PF_GROW,
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

typedef Uint8 SolidFlags;
#define SOL_SOLID (SolidFlags)(1U << 0)
#define SOL_TOP (SolidFlags)(1U << 1)
#define SOL_BOTTOM (SolidFlags)(1U << 2)
#define SOL_LEFT (SolidFlags)(1U << 3)
#define SOL_RIGHT (SolidFlags)(1U << 4)
#define SOL_SLOPE (SolidFlags)(1U << 5)
#define SOL_SLOPE_LEFT (SOL_SLOPE | SOL_RIGHT)
#define SOL_SLOPE_RIGHT (SOL_SLOPE | SOL_LEFT)

typedef Uint8 TouchFlags;
#define TOUCH_LEFT (TouchFlags)(1U << 0)
#define TOUCH_RIGHT (TouchFlags)(1U << 1)
#define TOUCH_TOP (TouchFlags)(1U << 2)
#define TOUCH_BOTTOM (TouchFlags)(1U << 3)
#define TOUCH_DISPLACEABLE (TouchFlags)(1U << 4)
#define TOUCH_SIDES (TOUCH_LEFT | TOUCH_RIGHT | TOUCH_TOP | TOUCH_BOTTOM)
#define TOUCH_STUCK TOUCH_SIDES

#define TOUCHING(actor, tuff) (((actor)->touch & (tuff)) != 0)
#define TOUCH_ON(actor, tuff) ((actor)->touch |= (tuff))
#define TOUCH_OFF(actor, tuff) ((actor)->touch &= ~(tuff))

typedef struct {
    const Fixed steer, jump;
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
    Uint64 seed;

    GamePlayerContext players[MAX_PLAYERS];
} GameContext;

typedef struct {
    GameSequenceType type;
    PlayerID activator;
    Uint16 time;
    Sint16 state;
} GameSequence;

typedef struct {
    PlayerID id;
    GameInput input, last_input;

    Sint8 lives;
    Uint8 coins;
    PlayerPowerup powerup;

    Uint8 track;
    ActorID actor;
    Fixed xscroll;
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

#define FOR_EACH_ACTOR(AAA)                                                                                            \
    for ((AAA) = get_actor(gamestate()->live_actors); (AAA) != NULL; (AAA) = get_actor((AAA)->previous))

#define BOX_OUTLINE_LEFT(actor)                                                                                        \
    ((FRect){                                                                                                          \
        {(actor)->pos.x + (actor)->box.start.x,       (actor)->pos.y + (actor)->box.start.y},                                \
        {(actor)->pos.x + (actor)->box.start.x + Fx1, (actor)->pos.y + (actor)->box.end.y  },                            \
    })
#define BOX_OUTLINE_RIGHT(actor)                                                                                       \
    ((FRect){                                                                                                          \
        {(actor)->pos.x + (actor)->box.end.x - Fx1, (actor)->pos.y + (actor)->box.start.y},                            \
        {(actor)->pos.x + (actor)->box.end.x,       (actor)->pos.y + (actor)->box.end.y  },                                    \
    })

typedef Uint32 ActorFlags;
#define FLG_VISIBLE (ActorFlags)(1U << 0)
#define FLG_DESTROY (ActorFlags)(1U << 1)
#define FLG_X_FLIP (ActorFlags)(1U << 2)
#define FLG_Y_FLIP (ActorFlags)(1U << 3)
#define FLG_FREEZE (ActorFlags)(1U << 4)
#define CUSTOM_FLAG(idx) (ActorFlags)(1U << (5 + (idx)))

typedef struct {
    ActorType type;
    PlayerID player;

    TouchFlags touch;
    Uint8 sprout;

    ActorID id;
    ActorID previous, next;
    ActorID platform;

    ActorID previous_cell, next_cell;
    Sint32 cell;

    FVec2 pos, last_pos, vel;
    FRect box;
    Fixed depth;

    ActorFlag flags;
    ActorValue values[MAX_VALUES];
} GameActor;

typedef struct {
    GameFlags flags;

    ActorID spawn, checkpoint, autoscroll, water;
    ActorID live_actors, next_actor;
    ActorID grid[GRID_SIZE];

    Sint16 clock;
    Uint64 seed;
    Uint64 time;

    GameSequence sequence;
    GamePlayer players[MAX_PLAYERS];
    GameActor actors[MAX_ACTORS];
} GameState;

SolidFlags always_solid(const GameActor*), always_top(const GameActor*), always_bottom(const GameActor*);

typedef struct {
    SolidFlags (*is_solid)(const GameActor*);

    void (*load)(), (*load_special)(const GameActor*);

    void (*create)(GameActor*);

    void (*pre_tick)(GameActor*), (*tick)(GameActor*), (*post_tick)(GameActor*);

    void (*draw)(const GameActor*), (*draw_dead)(const GameActor*), (*draw_hud)(const GameActor*);

    void (*cleanup)(GameActor*);

    void (*collide)(GameActor*, GameActor*);
    void (*on_left)(GameActor*, GameActor*), (*on_top)(GameActor*, GameActor*), (*on_right)(GameActor*, GameActor*),
        (*on_bottom)(GameActor*, GameActor*);
} ActorTable;

typedef struct {
    Uint16 size[2];
    FVec2 cell_size;
    FRect bounds;
    SolidFlags* grid;
} CollisionMap;

typedef Uint8 GameWarpID;
#define MAX_GAME_WARPS 4

typedef Uint8 GameStringID;
#define MAX_GAME_TRACKS 4
#define MAX_GAME_SECRETS 4
enum {
    GSTR_LABEL,
    GSTR_TRACK_START,
    GSTR_TRACK_END = GSTR_TRACK_START + MAX_GAME_TRACKS - 1,
    GSTR_SECRET_START,
    GSTR_SECRET_END = GSTR_SECRET_START + MAX_GAME_SECRETS - 1,
    GSTR_SIZE,
};

typedef struct {
    Uint8 bro_throw;
    FVec2 size, bowser_bounds, cheep_bounds;
    FRect bounds;

    TinyHash warps[MAX_GAME_WARPS];
    const char* strings[GSTR_SIZE];
    Uint32 track_offsets[MAX_GAME_TRACKS];

    Uint8 num_collisions;
    CollisionMap* collisions;
} LevelInfo;

void game_init();

Uint32 get_game_hash();
void recalculate_game_hash();

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
void pre_interp_game(), interp_game();

const GameContext* gamecontext();
const LevelInfo* levelinfo();
GameState* gamestate();

const char* get_game_secret(Uint8);

PlayerID localplayer(), viewplayer();
void set_view_player(const GamePlayer*);

GameSequence* get_sequence();
void set_sequence(GameSequenceType, const GamePlayer*, Sint16);
Bool in_blocking_sequence();

GamePlayer* get_player(PlayerID);
GameActor* respawn_player(GamePlayer*);
Fixed get_player_jump(const GamePlayer*);
const FVec2 nearest_player_pos(const FVec2);
void set_player_track(GamePlayer*, Uint8), update_player_track(const GamePlayer*);
Bool all_players_dead();
void win_player(GamePlayer*);

void load_actor(ActorType);
GameActor *create_actor(ActorType, const FVec2), *get_actor(ActorID);
ActorID get_num_actors(ActorType);
void replace_actors(ActorType, ActorType);

void move_actor(GameActor*, const FVec2), push_actors(GameActor*);

Bool in_any_view(const FVec2, Fixed, Bool), in_player_view(const GamePlayer*, const FVec2, Fixed, Bool);
Bool in_any_x_view(Fixed, Fixed), in_player_x_view(const GamePlayer*, Fixed, Fixed);
Bool below_nearest_bounds(const FVec2, Fixed), below_nearest_view(const FVec2, Fixed);

void collide_actor(GameActor*);
Bool touching_solid(const FRect, SolidFlags);
void displace_actor(GameActor*, Fixed, Bool), displace_actor_soft(GameActor*);

void draw_actor(const GameActor*, const char*, Bool);
void draw_dead_actor(const GameActor*);
void quake_at_actor(const GameActor*, float);

Sint32 rng(Sint32);

const FVec2 get_interp(const GameActor*);
void skip_interp(const GameActor*);
void align_interp(const GameActor*, const GameActor*);
