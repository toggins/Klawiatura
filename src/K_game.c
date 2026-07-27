#include "K_audio.h"
#include "K_cmd.h"
#include "K_game.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_net.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "actors/K_checkpoint.h"

#define ACTOR_CALL_STATIC(type, fn, ...)                                                                               \
    do {                                                                                                               \
        if (ACTORS[(type)] != NULL && ACTORS[(type)]->fn != NULL)                                                      \
            ACTORS[(type)]->fn(__VA_ARGS__);                                                                           \
    } while (FALSE)

#define ACTOR_CALL(act, fn, ...)                                                                                       \
    do {                                                                                                               \
        if (ACTORS[(act)->type] != NULL && ACTORS[(act)->type]->fn != NULL)                                            \
            ACTORS[(act)->type]->fn((act), ##__VA_ARGS__);                                                             \
    } while (FALSE)

#define ACTOR_GET_SOLID(act)                                                                                           \
    ((ACTORS[(act)->type] != NULL && ACTORS[(act)->type]->is_solid != NULL) ? ACTORS[(act)->type]->is_solid(act) : 0)

#define ACTOR_IS_SOLID(act, types) (ACTOR_GET_SOLID(act) & (types))

typedef struct {
    GameState game;
    AudioState audio;
} SaveState;

// `extern` in K_actors.c
const ActorTable* ACTORS[ACT_SIZE] = {0};

static const GameCharacter CHARACTERS[CHR_SIZE] = {
    [CHR_MARIO] = {
        .name = "Mario",
        .steer = Fx1,
        .jump = Fx1,
        .cursor = "characters/mario/cursor/%u",
        .sprites = {
            [POW_NONE] = {
                [PF_IDLE] = "characters/mario/small/idle",
                [PF_WALK1] = "characters/mario/small/walk0",
                [PF_WALK2] = "characters/mario/small/walk1",
                [PF_WALK3] = "characters/mario/small/walk2",
                [PF_JUMP] = "characters/mario/small/jump",
                [PF_FALL] = "characters/mario/small/fall",
                [PF_SWIM1] = "characters/mario/small/swim0",
                [PF_SWIM2] = "characters/mario/small/swim1",
                [PF_SWIM3] = "characters/mario/small/swim2",
                [PF_SWIM4] = "characters/mario/small/swim3",
                [PF_SWIM5] = "characters/mario/small/swim4",
                [PF_SWIM6] = "characters/mario/small/swim5",
                [PF_SWIM7] = "characters/mario/small/swim6",
                [PF_SWIM8] = "characters/mario/small/swim7",
                [PF_GROW1] = "characters/mario/small/grow0",
                [PF_GROW2] = "characters/mario/small/grow1",
                [PF_GROW3] = "characters/mario/small/grow2",
                [PF_DEAD] = "characters/mario/small/dead",
            },
            [POW_SUPER_MUSHROOM] = {
                [PF_IDLE] = "characters/mario/super/idle",
                [PF_WALK1] = "characters/mario/super/walk0",
                [PF_WALK2] = "characters/mario/super/walk1",
                [PF_WALK3] = "characters/mario/super/walk2",
                [PF_JUMP] = "characters/mario/super/jump",
                [PF_FALL] = "characters/mario/super/fall",
                [PF_DUCK] = "characters/mario/super/duck",
                [PF_SWIM1] = "characters/mario/super/swim0",
                [PF_SWIM2] = "characters/mario/super/swim1",
                [PF_SWIM3] = "characters/mario/super/swim2",
                [PF_SWIM4] = "characters/mario/super/swim3",
                [PF_SWIM5] = "characters/mario/super/swim4",
                [PF_SWIM6] = "characters/mario/super/swim5",
                [PF_SWIM7] = "characters/mario/super/swim6",
                [PF_SWIM8] = "characters/mario/super/swim7",
                [PF_GROW1] = "characters/mario/super/grow0",
                [PF_GROW2] = "characters/mario/super/grow1",
                [PF_GROW3] = "characters/mario/super/grow2",
            },
            [POW_FIRE_FLOWER] = {
                [PF_IDLE] = "characters/mario/fire/idle",
                [PF_WALK1] = "characters/mario/fire/walk0",
                [PF_WALK2] = "characters/mario/fire/walk1",
                [PF_WALK3] = "characters/mario/fire/walk2",
                [PF_JUMP] = "characters/mario/fire/jump",
                [PF_FALL] = "characters/mario/fire/fall",
                [PF_DUCK] = "characters/mario/fire/duck",
                [PF_FIRE1] = "characters/mario/fire/fire0",
                [PF_FIRE2] = "characters/mario/fire/fire1",
                [PF_SWIM1] = "characters/mario/fire/swim0",
                [PF_SWIM2] = "characters/mario/fire/swim1",
                [PF_SWIM3] = "characters/mario/fire/swim2",
                [PF_SWIM4] = "characters/mario/fire/swim3",
                [PF_SWIM5] = "characters/mario/fire/swim4",
                [PF_SWIM6] = "characters/mario/fire/swim5",
                [PF_SWIM7] = "characters/mario/fire/swim6",
                [PF_SWIM8] = "characters/mario/fire/swim7",
                [PF_GROW1] = "characters/mario/fire/grow0",
                [PF_GROW2] = "characters/mario/fire/grow1",
                [PF_GROW3] = "characters/mario/fire/grow2",
                [PF_GROW4] = "characters/mario/fire/grow3",
            },
            [POW_BEETROOT] = {
                [PF_IDLE] = "characters/mario/beetroot/idle",
                [PF_WALK1] = "characters/mario/beetroot/walk0",
                [PF_WALK2] = "characters/mario/beetroot/walk1",
                [PF_WALK3] = "characters/mario/beetroot/walk2",
                [PF_JUMP] = "characters/mario/beetroot/jump",
                [PF_FALL] = "characters/mario/beetroot/fall",
                [PF_DUCK] = "characters/mario/beetroot/duck",
                [PF_FIRE1] = "characters/mario/beetroot/fire0",
                [PF_FIRE2] = "characters/mario/beetroot/fire1",
                [PF_SWIM1] = "characters/mario/beetroot/swim0",
                [PF_SWIM2] = "characters/mario/beetroot/swim1",
                [PF_SWIM3] = "characters/mario/beetroot/swim2",
                [PF_SWIM4] = "characters/mario/beetroot/swim3",
                [PF_SWIM5] = "characters/mario/beetroot/swim4",
                [PF_SWIM6] = "characters/mario/beetroot/swim5",
                [PF_SWIM7] = "characters/mario/beetroot/swim6",
                [PF_SWIM8] = "characters/mario/beetroot/swim7",
                [PF_GROW1] = "characters/mario/beetroot/grow0",
                [PF_GROW2] = "characters/mario/beetroot/grow1",
                [PF_GROW3] = "characters/mario/beetroot/grow2",
                [PF_GROW4] = "characters/mario/beetroot/grow3",
            },
            [POW_GREEN_LUI] = {
                [PF_IDLE] = "characters/mario/lui/idle",
                [PF_WALK1] = "characters/mario/lui/walk0",
                [PF_WALK2] = "characters/mario/lui/walk1",
                [PF_WALK3] = "characters/mario/lui/walk2",
                [PF_JUMP] = "characters/mario/lui/jump",
                [PF_FALL] = "characters/mario/lui/fall",
                [PF_DUCK] = "characters/mario/lui/duck",
                [PF_SWIM1] = "characters/mario/lui/swim0",
                [PF_SWIM2] = "characters/mario/lui/swim1",
                [PF_SWIM3] = "characters/mario/lui/swim2",
                [PF_SWIM4] = "characters/mario/lui/swim3",
                [PF_SWIM5] = "characters/mario/lui/swim4",
                [PF_SWIM6] = "characters/mario/lui/swim5",
                [PF_SWIM7] = "characters/mario/lui/swim6",
                [PF_SWIM8] = "characters/mario/lui/swim7",
                [PF_GROW1] = "characters/mario/lui/grow0",
                [PF_GROW2] = "characters/mario/lui/grow1",
                [PF_GROW3] = "characters/mario/lui/grow2",
                [PF_GROW4] = "characters/mario/lui/grow3",
            }
        },
        .voices = {
            [PV_READY] = "vo/mario/ready",
            [PV_CHECKPOINT1] = "vo/mario/checkpoint0",
            [PV_CHECKPOINT2] = "vo/mario/checkpoint1",
            [PV_CHECKPOINT3] = "vo/mario/checkpoint2",
            [PV_PANIC] = "vo/mario/panic",
        }
    },

    [CHR_LUIGI] = {
        .name = "Luigi",
        .steer = 53248,
        .jump = 70577,
        .cursor = "characters/luigi/cursor/%u",
        .sprites = {
            [POW_NONE] = {
                [PF_IDLE] = "characters/luigi/small/idle",
                [PF_WALK1] = "characters/luigi/small/walk0",
                [PF_WALK2] = "characters/luigi/small/walk1",
                [PF_WALK3] = "characters/luigi/small/walk2",
                [PF_JUMP] = "characters/luigi/small/jump",
                [PF_FALL] = "characters/luigi/small/fall",
                [PF_SWIM1] = "characters/luigi/small/swim0",
                [PF_SWIM2] = "characters/luigi/small/swim1",
                [PF_SWIM3] = "characters/luigi/small/swim2",
                [PF_SWIM4] = "characters/luigi/small/swim3",
                [PF_SWIM5] = "characters/luigi/small/swim4",
                [PF_SWIM6] = "characters/luigi/small/swim5",
                [PF_SWIM7] = "characters/luigi/small/swim6",
                [PF_SWIM8] = "characters/luigi/small/swim7",
                [PF_GROW1] = "characters/luigi/small/grow0",
                [PF_GROW2] = "characters/luigi/small/grow1",
                [PF_GROW3] = "characters/luigi/small/grow2",
                [PF_DEAD] = "characters/luigi/small/dead",
            },
            [POW_SUPER_MUSHROOM] = {
                [PF_IDLE] = "characters/luigi/super/idle",
                [PF_WALK1] = "characters/luigi/super/walk0",
                [PF_WALK2] = "characters/luigi/super/walk1",
                [PF_WALK3] = "characters/luigi/super/walk2",
                [PF_JUMP] = "characters/luigi/super/jump",
                [PF_FALL] = "characters/luigi/super/fall",
                [PF_DUCK] = "characters/luigi/super/duck",
                [PF_SWIM1] = "characters/luigi/super/swim0",
                [PF_SWIM2] = "characters/luigi/super/swim1",
                [PF_SWIM3] = "characters/luigi/super/swim2",
                [PF_SWIM4] = "characters/luigi/super/swim3",
                [PF_SWIM5] = "characters/luigi/super/swim4",
                [PF_SWIM6] = "characters/luigi/super/swim5",
                [PF_SWIM7] = "characters/luigi/super/swim6",
                [PF_SWIM8] = "characters/luigi/super/swim7",
                [PF_GROW1] = "characters/luigi/super/grow0",
                [PF_GROW2] = "characters/luigi/super/grow1",
                [PF_GROW3] = "characters/luigi/super/grow2",
            },
            [POW_FIRE_FLOWER] = {
                [PF_IDLE] = "characters/luigi/fire/idle",
                [PF_WALK1] = "characters/luigi/fire/walk0",
                [PF_WALK2] = "characters/luigi/fire/walk1",
                [PF_WALK3] = "characters/luigi/fire/walk2",
                [PF_JUMP] = "characters/luigi/fire/jump",
                [PF_FALL] = "characters/luigi/fire/fall",
                [PF_DUCK] = "characters/luigi/fire/duck",
                [PF_FIRE1] = "characters/luigi/fire/fire0",
                [PF_FIRE2] = "characters/luigi/fire/fire1",
                [PF_SWIM1] = "characters/luigi/fire/swim0",
                [PF_SWIM2] = "characters/luigi/fire/swim1",
                [PF_SWIM3] = "characters/luigi/fire/swim2",
                [PF_SWIM4] = "characters/luigi/fire/swim3",
                [PF_SWIM5] = "characters/luigi/fire/swim4",
                [PF_SWIM6] = "characters/luigi/fire/swim5",
                [PF_SWIM7] = "characters/luigi/fire/swim6",
                [PF_SWIM8] = "characters/luigi/fire/swim7",
                [PF_GROW1] = "characters/luigi/fire/grow0",
                [PF_GROW2] = "characters/luigi/fire/grow1",
                [PF_GROW3] = "characters/luigi/fire/grow2",
                [PF_GROW4] = "characters/luigi/fire/grow3",
            },
            [POW_BEETROOT] = {
                [PF_IDLE] = "characters/luigi/beetroot/idle",
                [PF_WALK1] = "characters/luigi/beetroot/walk0",
                [PF_WALK2] = "characters/luigi/beetroot/walk1",
                [PF_WALK3] = "characters/luigi/beetroot/walk2",
                [PF_JUMP] = "characters/luigi/beetroot/jump",
                [PF_FALL] = "characters/luigi/beetroot/fall",
                [PF_DUCK] = "characters/luigi/beetroot/duck",
                [PF_FIRE1] = "characters/luigi/beetroot/fire0",
                [PF_FIRE2] = "characters/luigi/beetroot/fire1",
                [PF_SWIM1] = "characters/luigi/beetroot/swim0",
                [PF_SWIM2] = "characters/luigi/beetroot/swim1",
                [PF_SWIM3] = "characters/luigi/beetroot/swim2",
                [PF_SWIM4] = "characters/luigi/beetroot/swim3",
                [PF_SWIM5] = "characters/luigi/beetroot/swim4",
                [PF_SWIM6] = "characters/luigi/beetroot/swim5",
                [PF_SWIM7] = "characters/luigi/beetroot/swim6",
                [PF_SWIM8] = "characters/luigi/beetroot/swim7",
                [PF_GROW1] = "characters/luigi/beetroot/grow0",
                [PF_GROW2] = "characters/luigi/beetroot/grow1",
                [PF_GROW3] = "characters/luigi/beetroot/grow2",
                [PF_GROW4] = "characters/luigi/beetroot/grow3",
            },
            [POW_GREEN_LUI] = {
                [PF_IDLE] = "characters/luigi/lui/idle",
                [PF_WALK1] = "characters/luigi/lui/walk0",
                [PF_WALK2] = "characters/luigi/lui/walk1",
                [PF_WALK3] = "characters/luigi/lui/walk2",
                [PF_JUMP] = "characters/luigi/lui/jump",
                [PF_FALL] = "characters/luigi/lui/fall",
                [PF_DUCK] = "characters/luigi/lui/duck",
                [PF_SWIM1] = "characters/luigi/lui/swim0",
                [PF_SWIM2] = "characters/luigi/lui/swim1",
                [PF_SWIM3] = "characters/luigi/lui/swim2",
                [PF_SWIM4] = "characters/luigi/lui/swim3",
                [PF_SWIM5] = "characters/luigi/lui/swim4",
                [PF_SWIM6] = "characters/luigi/lui/swim5",
                [PF_SWIM7] = "characters/luigi/lui/swim6",
                [PF_SWIM8] = "characters/luigi/lui/swim7",
                [PF_GROW1] = "characters/luigi/lui/grow0",
                [PF_GROW2] = "characters/luigi/lui/grow1",
                [PF_GROW3] = "characters/luigi/lui/grow2",
                [PF_GROW4] = "characters/luigi/lui/grow3",
            }
        },
        .voices = {
            [PV_READY] = "vo/luigi/ready",
            [PV_CHECKPOINT1] = "vo/luigi/checkpoint0",
            [PV_CHECKPOINT2] = "vo/luigi/checkpoint1",
            [PV_CHECKPOINT3] = "vo/luigi/checkpoint2",
            [PV_PANIC] = "vo/luigi/panic",
        }
    },
};

static Uint32 game_hash = 0;

static PlayerID local_player = NULL_PLAYER, view_player = NULL_PLAYER;

static TinyPq depth_sorter = {0};
static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;

GameContext queue_game_context = {0};
static GameContext game_context = {0};

static LevelInfo* level_info = NULL;
static GameState* game_state = NULL;
static InterpActor* interp_actors = NULL;

static Uint8 boot_state = 0;
static char boot_reason[256] = "";

void game_init() {
    extern void POPULATE_ACTORS_TABLE();
    POPULATE_ACTORS_TABLE();

    extern void CALCULATE_GAME_HASH(Uint32*);
    CALCULATE_GAME_HASH(&game_hash);
}

Uint32 get_game_hash() {
    return game_hash;
}

const GameCharacter* get_character(PlayerCharacter cid) {
    return (cid < 0 || cid >= CHR_SIZE) ? NULL : &CHARACTERS[cid];
}

const char* get_character_name(PlayerCharacter cid) {
    const GameCharacter* character = get_character(cid);
    return (character == NULL) ? NULL : character->name;
}

const char* get_character_cursor(PlayerCharacter cid) {
    return (cid < 0 || cid >= CHR_SIZE || CHARACTERS[cid].cursor == NULL) ? "%u" : CHARACTERS[cid].cursor;
}

const char* get_character_sprite(PlayerCharacter cid, PlayerPowerup powerup, PlayerFrame frame) {
    return (cid < 0 || cid >= CHR_SIZE || powerup < 0 || powerup >= POW_SIZE || frame < 0 || frame >= PF_SIZE)
               ? NULL
               : CHARACTERS[cid].sprites[powerup][frame];
}

const char* get_character_voice(PlayerCharacter cid, PlayerVoice voice) {
    return (cid < 0 || cid >= CHR_SIZE || voice < 0 || voice >= PV_SIZE) ? NULL : CHARACTERS[cid].voices[voice];
}

const char* get_powerup_name(PlayerPowerup powerup) {
    switch (powerup) {
    default:
        return LFMT("val_none");
    case POW_SUPER_MUSHROOM:
        return LFMT("val_super_mushroom");
    case POW_FIRE_FLOWER:
        return LFMT("val_fire_flower");
    case POW_BEETROOT:
        return LFMT("val_beetroot");
    case POW_GREEN_LUI:
        return LFMT("val_green_lui");
    }
}

Sint8 get_powerup_cost(PlayerPowerup powerup) {
    switch (powerup) {
    default:
        return 0;
    case POW_SUPER_MUSHROOM:
        return 1;
    case POW_FIRE_FLOWER:
    case POW_BEETROOT:
    case POW_GREEN_LUI:
        return 2;
    }
}

void poll_game() {
    if (game_session != NULL)
        gekko_network_poll(game_session);
}

float frames_ahead() {
    return (game_session == NULL) ? 0.f : gekko_frames_ahead(game_session);
}

void boot_from_game(const char* reason) {
    if (game_session == NULL || boot_state > 1)
        return;

    boot_state = 1;
    if (reason == NULL)
        boot_reason[0] = '\0';
    else
        SDL_strlcpy(boot_reason, reason, sizeof(boot_reason));
    WARN("Game boot imminent: %s", boot_reason);
}

// =======
// CONTEXT
// =======

GameContext empty_game_context() {
    GameContext ctx = {0};

    ctx.checkpoint = NULL_ACTOR;

    ctx.num_players = 1;
    for (PlayerID i = 0; i < (PlayerID)SDL_arraysize(ctx.players); i++)
        ctx.players[i].lives = DEFAULT_LIVES;

    return ctx;
}

GameContext init_game_context(const WorldContext* wctx, TinyHash level) {
    GameContext gctx = empty_game_context();
    gctx.level = level;

    if (wctx != NULL) {
        gctx.num_players = wctx->num_players;
        for (size_t i = 0; i < SDL_arraysize(gctx.players); i++) {
            GamePlayerContext* gpctx = &gctx.players[i];
            const WorldPlayerContext* wpctx = &wctx->players[i];

            gpctx->character = wpctx->character;
            gpctx->powerup = wpctx->powerup;
            gpctx->lives = wpctx->lives;
            gpctx->coins = wpctx->coins;
            gpctx->score = wpctx->score;
        }
    }

    if (is_connected())
        for (PlayerID i = 0, n = (PlayerID)get_game_player_count(); i < n; i++)
            gctx.players[i].xscroll = get_peer_bool(player_to_peer(i), "xscroll");
    else
        gctx.players[0].xscroll = CLIENT.xscroll;

    return gctx;
}

void jump_to_game(const GameContext* ctx, Bool as_host) {
    if (ctx == NULL || (as_host && !is_host()) || (!as_host && !is_leader()))
        return;

    const Level* level = get_level_key(ctx->level);
    ASSUME(level, "Invalid level key %" SDL_PRIu64, ctx->level);

    spread_game_packet(ctx);
    set_screen(SCR_GAME, ctx, sizeof(*ctx));
}

// =====
// STATE
// =====

static void start_game_state() {
    // Allocate state
    game_state = SDL_calloc(1, sizeof(*game_state));
    EXPECT(game_state, "Failed to allocate game state");

    // Nullify entire state
    for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
        GamePlayer* player = &game_state->players[i];

        player->id = NULL_PLAYER;
        player->actor = NULL_ACTOR;
        for (ActorID j = 0; j < MAX_PROJECTILES; j++)
            player->projectiles[j] = NULL_ACTOR;
        for (ActorID j = 0; j < MAX_SINKING_PROJECTILES; j++)
            player->sinking_projectiles[j] = NULL_ACTOR;

        player->lives = DEFAULT_LIVES;
        player->track = 255;
    }

    game_state->live_actors = NULL_ACTOR;
    for (ActorID i = 0; i < MAX_ACTORS; i++) {
        GameActor* actor = &game_state->actors[i];

        actor->id = actor->previous = actor->next = actor->previous_cell = actor->next_cell = NULL_ACTOR;
        actor->player = NULL_PLAYER;
        VAL(actor, PLATFORM) = NULL_ACTOR;
    }
    for (Sint32 i = 0; i < GRID_SIZE; i++)
        game_state->grid[i] = NULL_ACTOR;

    game_state->spawn = game_state->checkpoint = game_state->water = NULL_ACTOR;

    game_state->clock = -1;

    game_state->sequence.activator = NULL_PLAYER;

    // RNG seed is determined by game context
    for (Uint64 i = 0; i < sizeof(game_context); i++)
        game_state->seed += ((Uint8*)&game_context)[i];
    INFO("Level seed is %" SDL_PRIu64, game_state->seed);

    // Level info
    level_info = SDL_calloc(1, sizeof(*level_info));
    EXPECT(level_info, "Failed to allocate level info");

    level_info->size.x = level_info->bounds.end.x = F_SCREEN_WIDTH;
    level_info->size.y = level_info->bounds.end.y = F_SCREEN_HEIGHT;
}

static void load_level(TinyHash key) {
    const Level* level = get_level_key(key);
    EXPECT(level, "Invalid level key %" SDL_PRIu64, key);

    const char* error = NULL;
    yyjson_doc* json = load_level_json(level->name, &error);
    EXPECT(json, "Failed to load level \"%s\": %s", level->name, error);

    yyjson_val* root = yyjson_doc_get_root(json);

    yyjson_val *jval = yyjson_obj_get(root, "strings"), *jval2 = NULL;

#define PARSE_STRING(I, N)                                                                                             \
    jval2 = yyjson_obj_get(jval, N);                                                                                   \
    if (yyjson_is_str(jval2)) {                                                                                        \
        level_info->strings[I] = SDL_strdup(yyjson_get_str(jval2));                                                    \
        EXPECT(level_info->strings[I], "Failed to allocate level \"%s\" string \"" #N "\"", level->name);              \
                                                                                                                       \
        if ((I) >= GSTR_TRACK_START && (I) <= GSTR_TRACK_END)                                                          \
            load_track(level_info->strings[I], AKL_NEVER);                                                             \
    }

    PARSE_STRING(GSTR_LABEL, "label");
    PARSE_STRING(GSTR_TRACK1, "track1");
    PARSE_STRING(GSTR_TRACK2, "track2");
    PARSE_STRING(GSTR_TRACK3, "track3");
    PARSE_STRING(GSTR_TRACK4, "track4");
    PARSE_STRING(GSTR_SECRET1, "secret1");
    PARSE_STRING(GSTR_SECRET2, "secret2");
    PARSE_STRING(GSTR_SECRET3, "secret3");
    PARSE_STRING(GSTR_SECRET4, "secret4");

#undef PARSE_STRING

    jval = yyjson_obj_get(root, "warps");
    size_t narr = yyjson_arr_size(jval);
    for (GameWarpID i = 0, n = SDL_min(narr, MAX_GAME_WARPS); i < n; i++)
        level_info->warps[i] = StHashStr(yyjson_get_str(yyjson_arr_get(jval, i)));

    jval = yyjson_obj_get(root, "size");
    level_info->size.x = Int2Fx(yyjson_get_uint(yyjson_arr_get(jval, 0)));
    level_info->size.y = Int2Fx(yyjson_get_uint(yyjson_arr_get(jval, 1)));

    jval = yyjson_obj_get(root, "bounds");
    level_info->bounds.start.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 0)));
    level_info->bounds.start.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 1)));
    level_info->bounds.end.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 2)));
    level_info->bounds.end.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 3)));

    jval = yyjson_obj_get(root, "time");
    if (yyjson_is_int(jval))
        game_state->clock = (Sint32)yyjson_get_sint(jval);

    jval = yyjson_obj_get(root, "water");
    if (yyjson_is_int(jval))
        game_state->water = Int2Fx(yyjson_get_sint(jval));

    read_tilemap(videostate()->tilemap, yyjson_obj_get(root, "backdrops"));

    jval = yyjson_obj_get(root, "actors");
    narr = yyjson_arr_size(jval);
    for (ActorID i = 0, n = SDL_min(narr, MAX_ACTORS); i < n; i++) {
        jval2 = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jval2))
            continue;

        const ActorType type = yyjson_get_uint(yyjson_obj_get(jval2, "id"));

        yyjson_val* jpos = yyjson_obj_get(jval2, "pos");
        const FVec2 pos = {
            Int2Fx(yyjson_get_sint(yyjson_arr_get(jpos, 0))),
            Int2Fx(yyjson_get_sint(yyjson_arr_get(jpos, 1))),
        };

        GameActor* actor = create_actor(type, pos);
        if (actor == NULL)
            continue;

        if (yyjson_arr_size(jpos) > 2)
            actor->depth = Int2Fx(yyjson_get_sint(yyjson_arr_get(jpos, 2)));

        yyjson_val* jscale = yyjson_obj_get(jval2, "scale");
        if (yyjson_is_arr(jscale)) {
            actor->box = Rmul(actor->box, (FVec2){
                                              (Fixed)yyjson_get_sint(yyjson_arr_get(jscale, 0)),
                                              (Fixed)yyjson_get_sint(yyjson_arr_get(jscale, 1)),
                                          });
        }

        yyjson_val* jvalues = yyjson_obj_get(jval2, "values");
        const size_t nvarr = yyjson_arr_size(jvalues);
        for (ActorValue j = 0, n = SDL_min(nvarr, MAX_VALUES); j < n; j++) {
            yyjson_val* jvalue = yyjson_arr_get(jvalues, j);

            const ActorValue idx = (ActorValue)yyjson_get_uint(yyjson_arr_get(jvalue, 0));
            if (idx < 0 || idx >= MAX_VALUES) {
                WTF("Invalid index %i for actor %i type %u", idx, i, type);
                continue;
            }

            actor->values[idx] = (ActorValue)yyjson_get_sint(yyjson_arr_get(jvalue, 1));
        }

        actor->flags |= yyjson_get_uint(yyjson_obj_get(jval2, "flags"));

        load_actor(type);
        ACTOR_CALL(actor, load_special);
    }

    yyjson_doc_free(json);
}

void start_game(const GameContext* ctx) {
    nuke_game();

    // Context
    EXPECT(ctx, "Null game context?");
    EXPECT(ctx->num_players >= 1 && ctx->num_players <= MAX_PLAYERS,
        "Invalid player count in game context! Expected 1..%i, got %i", MAX_PLAYERS, ctx->num_players);
    game_context = *ctx;

    // States
    start_video_state();
    start_audio_state();
    start_game_state();

    interp_actors = SDL_calloc(MAX_ACTORS, sizeof(*interp_actors));
    EXPECT(interp_actors, "Failed to allocate interpolation actors");

    // Load assets
    load_sprite_num("ui/coins/%u", 3, AKL_NEVER);
    load_sprite("ui/bezel_l", AKL_NEVER);
    load_sprite("ui/bezel_r", AKL_NEVER);
    load_font("hud", AKL_NEVER);

    // Session
    const Bool spectating = i_am_spectating();
    gekko_create(&game_session, spectating ? GekkoSpectateSession : GekkoGameSession);

    GekkoConfig cfg = {0};
    cfg.num_players = game_context.num_players;
    cfg.input_size = sizeof(GameInput);
    cfg.state_size = sizeof(SaveState);
    cfg.input_prediction_window = MAX_INPUT_DELAY;
    cfg.desync_detection = TRUE;
    if (spectating)
        cfg.spectator_delay = MAX_INPUT_DELAY;
    else
        cfg.max_spectators = get_game_spectator_count();

    gekko_start(game_session, &cfg);
    gekko_set_disconnect_timeout(game_session, 0);
    local_player = populate_game(game_session, game_context.num_players);

    // Surface
    game_surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);

    // Initial game state
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        if (game_context.players[i].lives >= 0) {
            game_state->checkpoint = game_context.checkpoint;
            break;
        }
    }

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        GamePlayer* player = &game_state->players[i];
        player->id = i;

        const GamePlayerContext* pctx = &game_context.players[i];
        if (pctx->lives < 0)
            continue;

        player->powerup = pctx->powerup;
        player->lives = pctx->lives;
        player->coins = pctx->coins;
        player->score = pctx->score;
    }

    load_level(game_context.level);

    for (PlayerID i = 0; i < game_context.num_players; i++)
        respawn_player(get_player(i));
}

static void nuke_game_state() {
    SDL_free(game_state);
    game_state = NULL;

    if (level_info != NULL) {
        for (GameStringID i = 0; i < (GameStringID)GSTR_SIZE; i++)
            SDL_free((void*)level_info->strings[i]);

        SDL_free(level_info);
        level_info = NULL;
    }
}

static void save_game_state(GameState* gs) {
    *gs = *game_state;
}

static void load_game_state(const GameState* gs) {
    *game_state = *gs;
}

static Uint32 check_game_state() {
    Uint32 checksum = 0;
    const Uint8* data = (Uint8*)game_state;
    for (Uint32 i = 0; i < sizeof(*game_state); i++)
        checksum += data[i];
    return checksum;
}

static void destroy_actor(GameActor*);
static void tick_game_state(GameInput inputs[MAX_PLAYERS]) {
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        // Apply input
        player->last_input = player->input;
        player->input = (game_state->sequence.type == GS_WIN) ? 0 : inputs[i];
    }

    GameActor* actor = get_actor(game_state->live_actors);
    while (actor != NULL) {
        actor->last_pos = actor->pos;

        if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE)) {
            if (VAL(actor, SPROUT) > 0)
                --VAL(actor, SPROUT);
            else
                ACTOR_CALL(actor, pre_tick);
        }

        GameActor* next = get_actor(actor->previous);
        if (ANY_FLAG(actor, FLG_DESTROY))
            destroy_actor(actor);
        actor = next;
    }

#define TICK_LOOP(ticker)                                                                                              \
    actor = get_actor(game_state->live_actors);                                                                        \
    while (actor != NULL) {                                                                                            \
        if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE) && VAL(actor, SPROUT) <= 0)                                     \
            ACTOR_CALL(actor, ticker);                                                                                 \
                                                                                                                       \
        GameActor* next = get_actor(actor->previous);                                                                  \
        if (ANY_FLAG(actor, FLG_DESTROY))                                                                              \
            destroy_actor(actor);                                                                                      \
        actor = next;                                                                                                  \
    }

    TICK_LOOP(tick);
    TICK_LOOP(post_tick);

#undef TICK_LOOP

    GameSequence* sequence = &game_state->sequence;
    switch (sequence->type) {
    default:
        break;

    case GS_LOSE: {
        if (sequence->time <= 0)
            break;

        switch (sequence->time++) {
        default:
            break;

        case 1: {
            stop_state_track(MAX_STATE_TRACKS);
            play_state_track(STS_FANFARE, "smb/game_over", 0);
            break;
        }

        case 301: {
            game_state->flags |= GF_END;
            break;
        }
        }

        break;
    }
    }

    ++game_state->time;
}

void nuke_game() {
    nuke_game_state();
    nuke_audio_state();
    nuke_video_state();
    gekko_destroy(&game_session);
    game_session = NULL;
    destroy_surface(game_surface);
    game_surface = NULL;
    SDL_free(interp_actors);
    interp_actors = NULL;
    local_player = view_player = NULL_PLAYER;
    FreeTinyPq(&depth_sorter);
}

void tick_game() {
    if (game_session == NULL)
        return;

    if (get_replay_state() == RPS_PLAYING) {
        const GameInput* input = read_replay();
        if (input == NULL) {
            boot_to_menu(LFMT("msg_replay_ended"));
            return;
        }

        for (PlayerID i = 0; i < game_context.num_players; i++)
            gekko_add_local_input(game_session, i, (void*)&input[i]);
    } else {
        // Since this isn't a replay, we can freely change levels/worlds.
        if (game_state->flags & GF_END) {
            if (!is_leader())
                return;

            GameContext ctx = game_context;
            ctx.flags |= GF_RESTARTED;
            ctx.checkpoint = game_state->checkpoint;
            for (PlayerID i = 0; i < ctx.num_players; i++) {
                ctx.players[i].lives = game_state->players[i].lives;
                ctx.players[i].coins = game_state->players[i].coins;
                ctx.players[i].score = game_state->players[i].score;
                ctx.players[i].powerup = game_state->players[i].powerup;
            }

            jump_to_game(&ctx, FALSE);
            return;
        }

        // LOCAL PLAYER GLOSSARY:
        // 0 .. (MAX_PLAYERS - 1) = Solo/online.
        // MAX_PLAYERS or higher = Spectator.
        if (local_player < MAX_PLAYERS) {
            GameInput input = 0;
            if (topui() == NULL) {
                input |= kb_down(KB_UP) * GI_UP;
                input |= kb_down(KB_LEFT) * GI_LEFT;
                input |= kb_down(KB_DOWN) * GI_DOWN;
                input |= kb_down(KB_RIGHT) * GI_RIGHT;
                input |= kb_down(KB_JUMP) * GI_JUMP;
                input |= kb_down(KB_RUN) * GI_RUN;
                input |= kb_down(KB_FIRE) * GI_FIRE;
            }

            gekko_add_local_input(game_session, local_player, &input);
        }
    }

    net_flush();

    int count = 0;
    GekkoSessionEvent** events = gekko_session_events(game_session, &count);
    for (int i = 0; i < count; i++) {
        GekkoSessionEvent* event = events[i];
        switch (event->type) {
        case GekkoDesyncDetected: {
            struct GekkoDesynced desync = event->data.desynced;

            boot_to_menu(
                LFMT("msg_player_desynced", 's', get_peer_name(player_to_peer((PlayerID)desync.remote_handle))));

            WTF("Tick: %i", desync.frame);
            WTF("Local Checksum: %i", desync.local_checksum);
            WTF("Remote Checksum: %i", desync.remote_checksum);
            return;
        }

        case GekkoPlayerConnected: {
            struct GekkoConnected cn = event->data.connected;
            INFO("%s %i connected", (cn.handle >= MAX_PLAYERS) ? "Spectator" : "Player", cn.handle + 1);
            break;
        }

        case GekkoPlayerDisconnected: {
            struct GekkoDisconnected dc = event->data.disconnected;
            if (dc.handle >= get_game_player_count()) {
                const PlayerID handle = (PlayerID)(dc.handle - get_game_player_count());
                nuke_spectator_peer(spectator_to_peer(handle));
                WARN("Spectator %i disconnected", handle + 1);
                break;
            }

            boot_to_menu(LFMT("msg_player_disconnected", 's', get_peer_name(player_to_peer((PlayerID)dc.handle))));
            return;
        }

        default:
            break;
        }
    }

    count = 0;
    GekkoGameEvent** updates = gekko_update_session(game_session, &count);
    for (int i = 0; i < count; i++) {
        GekkoGameEvent* event = updates[i];
        switch (event->type) {
        case GekkoSaveEvent: {
            static SaveState save = {0};
            save_game_state(&save.game);
            save_audio_state(&save.audio);

            *event->data.save.state_len = sizeof(save);
            *event->data.save.checksum = check_game_state();
            SDL_memcpy(event->data.save.state, &save, sizeof(save));
            break;
        }

        case GekkoLoadEvent: {
            const SaveState* load = (SaveState*)(event->data.load.state);
            load_game_state(&load->game);
            load_audio_state(&load->audio);
            break;
        }

        case GekkoAdvanceEvent: {
            tick_game_state((GameInput*)event->data.adv.inputs);
            tick_video_state();
            tick_audio_state(event->data.adv.rolling_back);

            switch (get_replay_state()) {
            default:
                break;

            case RPS_RECORDING: {
                write_replay(event->data.adv.frame, (GameInput*)event->data.adv.inputs, check_game_state());
                break;
            }

            case RPS_PLAYING: {
                const Uint32 game_checksum = check_game_state(), replay_checksum = get_replay_checksum();
                if (game_checksum == replay_checksum)
                    break;

                WTF("REPLAY DESYNC");
                WTF("Game checksum: %u", game_checksum);
                WTF("Replay checksum: %u", replay_checksum);
                boot_to_menu(LFMT("msg_replay_desynced"));
                return;
            }
            }
            break;
        }

        default:
            break;
        }
    }
}

void pre_interp_game() {
    const GameActor* actor = NULL;

    const int fps = get_framerate();
    if (fps > 0 && fps <= TICKRATE) {
        FOR_EACH_ACTOR(actor) {
            InterpActor* iactor = &interp_actors[actor->id];
            iactor->type = actor->type;
            iactor->from = iactor->to = iactor->pos = actor->pos;
        }

        return;
    }

    FOR_EACH_ACTOR(actor) {
        InterpActor* iactor = &interp_actors[actor->id];
        if (iactor->type == actor->type) {
            iactor->from = iactor->to;
            iactor->to = actor->pos;
        } else {
            iactor->type = actor->type;
            iactor->from = iactor->to = iactor->pos = actor->pos;
        }
    }
}

void interp_game() {
    const int fps = get_framerate();
    if (fps > 0 && fps <= TICKRATE)
        return;

    const Fixed t = Float2Fx(pendingticks());
    const GameActor* actor = NULL;
    FOR_EACH_ACTOR(actor) {
        InterpActor* iactor = &interp_actors[actor->id];
        iactor->pos = Vlerp(iactor->from, iactor->to, t);
    }
}

static void draw_game_state() {
    static mat4 oview = GLM_MAT4_IDENTITY_INIT, view = GLM_MAT4_IDENTITY_INIT;

    get_view_matrix(oview);

    clear_depth(1.f);
    batch_filter(FALSE);

    const GamePlayer* player = get_player(view_player);

    VideoCamera* camera = &videostate()->camera;
    if (player != NULL) {
        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL) {
            camera->pos = Vclamp(
                get_interp(pawn), Vadd(player->bounds.start, F_HALF_SCREEN), Vsub(player->bounds.end, F_HALF_SCREEN));
        }
    }

    const Sint32 cx = Fx2Int(camera->pos.x), cy = Fx2Int(camera->pos.y);
    glm_look((vec3){(float)(cx - HALF_SCREEN_WIDTH), (float)(cy - HALF_SCREEN_HEIGHT), 0.f}, GLM_ZUP, GLM_YUP, view);
    set_view_matrix(view);
    apply_matrices();

    draw_tilemap(videostate()->tilemap);

    for (const GameActor* actor = get_actor(game_state->live_actors); actor != NULL; actor = get_actor(actor->previous))
        if (ANY_FLAG(actor, FLG_VISIBLE))
            TinyPqInsert(&depth_sorter, actor->depth, (const void*)&actor, sizeof(const GameActor**));

    TinyPqIterator iter = TinyPqIter(&depth_sorter);
    while (TinyPqNext(&iter)) {
        const GameActor* actor = *(const GameActor**)iter.data;
        ACTOR_CALL(actor, draw);
    }

    FreeTinyPq(&depth_sorter);

    set_view_matrix(oview);
    apply_matrices();

    if (player == NULL)
        goto dgs_no_hud;

    batch_write_depth(FALSE);
    batch_test_depth(FALSE);

    batch_reset();
    batch_pos(B_XY(32.f, 16.f));
    const char* cname = get_character_name(game_context.players[player->id].character);
    batch_string("hud", 16.f, fmt("%s × %i", cname, SDL_max(player->lives, 0)));
    batch_pos(B_XY(32.f + string_width("hud", 16.f, fmt("%s × 0", cname)), 34.f));
    batch_align(B_TOP_RIGHT);
    batch_string("hud", 16.f, fmt("%u", player->score));

    batch_pos(B_XY(224.f, 34.f));
    batch_sprite(fmt("ui/coins/%i", ((game_state->time * 4) / 25) % 3));
    batch_pos(B_XY(235.f, 34.f));
    batch_align(B_TOP_LEFT);
    batch_string("hud", 16.f, " ×");
    batch_pos(B_XY(285.f, 34.f));
    batch_align(B_TOP_RIGHT);
    batch_string("hud", 16.f, fmt("%u", player->coins));

    const char* label = level_info->strings[GSTR_LABEL];
    if (label != NULL) {
        switch (label[0]) {
        default: {
            batch_pos(B_XY(432.f, 16.f));
            batch_sprite(label);
            break;
        }

        case '@': {
            batch_pos(B_XY(432.f, 16.f));
            batch_align(B_ALIGN(FA_CENTER, FA_TOP));
            batch_string("hud", 16.f, "WORLD");
            batch_pos(B_XY(432.f, 34.f));
            batch_string("hud", 16.f, LFMT(label + 1));
            break;
        }

        case '$': {
            batch_pos(B_XY(432.f, 34.f));
            batch_align(B_CENTER);
            batch_string("hud", 16.f, LFMT(label + 1));
            break;
        }
        }
    }

    if (game_state->clock >= 0) {
        batch_pos(B_XY(SCREEN_WIDTH - 32.f, 16.f));
        batch_align(B_TOP_RIGHT);
        batch_string("hud", 16.f, "TIME");
        batch_pos(B_XY(SCREEN_WIDTH - 32.f, 34.f));
        batch_string("hud", 16.f, fmt("%i", game_state->clock));
    }

    const GameSequence* sequence = &game_state->sequence;
    if (sequence->type == GS_LOSE && sequence->time > 0) {
        batch_pos(B_HALF_SCREEN);
        batch_align(B_CENTER);
        batch_string("hud", 16.f, "GAME OVER");
    }

    batch_test_depth(TRUE);
    batch_write_depth(TRUE);

dgs_no_hud:
    batch_filter(TRUE);
}

void draw_game() {
    push_surface(game_surface);
    draw_game_state();
    pop_surface();

    batch_reset();
    batch_surface(game_surface);
    batch_sprite("ui/bezel_l");
    batch_sprite("ui/bezel_r");
}

const GameContext* gamecontext() {
    return &game_context;
}

const LevelInfo* levelinfo() {
    return level_info;
}

GameState* gamestate() {
    return game_state;
}

// =======
// PLAYERS
// =======

/// THIS IS A CLIENT-SIDE INDEX. DO NOT USE IN GAME STATE!!!
PlayerID localplayer() {
    return local_player;
}

/// THIS IS A CLIENT-SIDE INDEX. DO NOT USE IN GAME STATE!!!
PlayerID viewplayer() {
    return view_player;
}

/// !!! CLIENT-SIDE !!!
static void try_play_track(Uint8 track) {
    if (track < 0 || track > (GSTR_TRACK_END - GSTR_TRACK_START))
        stop_state_track(STS_MAIN);
    else
        play_state_track(STS_MAIN, level_info->strings[GSTR_TRACK_START + track], PLAY_LOOPING);
}

void set_view_player(const GamePlayer* player) {
    if (player == NULL || view_player == player->id)
        return;

    const GamePlayer* old_player = get_player(view_player);
    view_player = player->id;

    if (old_player == NULL || player->track != old_player->track)
        try_play_track(player->track);
}
/// !!! CLIENT-SIDE !!!

//
GamePlayer* get_player(PlayerID pid) {
    return (pid < 0 || pid >= MAX_PLAYERS || game_state->players[pid].id != pid) ? NULL : &game_state->players[pid];
}

GameActor* respawn_player(GamePlayer* player) {
    if (player == NULL || player->lives < 0)
        return NULL;

    const GameActor* spawn = get_actor(game_state->checkpoint);
    if (spawn == NULL)
        spawn = get_actor(game_state->spawn);
    if (spawn == NULL)
        return NULL;

    FVec2 spos = spawn->pos;
    if (spawn->type == ACT_CHECKPOINT) {
        spos = Vadd(spos, (FVec2){Int2Fx(53), Int2Fx(117)});

        const Fixed bx1 = VAL(spawn, CHECKPOINT_BOUNDS_X1), by1 = VAL(spawn, CHECKPOINT_BOUNDS_Y1),
                    bx2 = VAL(spawn, CHECKPOINT_BOUNDS_X2), by2 = VAL(spawn, CHECKPOINT_BOUNDS_Y2);
        player->bounds = (bx1 == bx2 && by1 == by2) ? level_info->bounds
                                                    : (FRect){
                                                          {bx1, by1},
                                                          {bx2, by2}
        };

        set_player_track(player, VAL(spawn, CHECKPOINT_TRACK));
    } else {
        player->bounds = level_info->bounds;
        set_player_track(player, 0);
    }

    GameActor* pawn = create_actor(ACT_PLAYER, spos);
    if (pawn == NULL)
        return NULL;

    pawn->player = player->id;
    FLAG_ON(pawn, spawn->flags & FLG_X_FLIP);

    player->actor = pawn->id;

    /// !!! CLIENT-SIDE !!!
    if (player->id == local_player || local_player >= MAX_PLAYERS)
        set_view_player(player);
    /// !!! CLIENT-SIDE !!!

    return pawn;
}

void set_player_track(GamePlayer* player, Uint8 track) {
    if (player == NULL || player->track == track)
        return;

    player->track = track;

    /// !!! CLIENT-SIDE !!!
    if (player->id == view_player)
        try_play_track(track);
    /// !!! CLIENT-SIDE !!!
}

Bool all_players_dead() {
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player != NULL && (player->lives >= 0 || get_actor(player->actor) != NULL))
            return FALSE;
    }

    return TRUE;
}

// ======
// ACTORS
// ======

SolidType always_solid(const GameActor* actor) {
    (void)actor;

    return SOL_SOLID;
}

SolidType always_top(const GameActor* actor) {
    (void)actor;

    return SOL_TOP;
}

SolidType always_bottom(const GameActor* actor) {
    (void)actor;

    return SOL_BOTTOM;
}

void load_actor(ActorType type) {
    if (type <= ACT_NULL || type >= ACT_SIZE)
        WARN("Loading invalid actor type %u", type);
    else
        ACTOR_CALL_STATIC(type, load);
}

GameActor* create_actor(ActorType type, const FVec2 pos) {
    if (type <= ACT_NULL || type >= ACT_SIZE) {
        WARN("Creating invalid actor type %u", type);
        return NULL;
    }

    ActorID index = game_state->next_actor;
    GameActor* actor = NULL;
    for (ActorID i = 0; i < MAX_ACTORS; i++) {
        actor = &game_state->actors[index];
        if (actor->id == NULL_ACTOR)
            goto found;

        index = (ActorID)((index + 1) % MAX_ACTORS);
    }

    WARN("Too many actors!!!");
    return NULL;

found:
    SDL_zerop(actor);

    actor->id = index;
    actor->type = type;
    actor->player = NULL_PLAYER;

    actor->previous = game_state->live_actors;
    actor->next = NULL_ACTOR;
    GameActor* first = get_actor(game_state->live_actors);
    if (first != NULL)
        first->next = index;
    game_state->live_actors = index;

    actor->cell = NULL_CELL;
    actor->previous_cell = actor->next_cell = NULL_ACTOR;
    move_actor(actor, pos);
    actor->last_pos = pos;

    VAL(actor, PLATFORM) = NULL_ACTOR;
    FLAG_ON(actor, FLG_VISIBLE);

    ACTOR_CALL(actor, create);
    skip_interp(actor);

    game_state->next_actor = (ActorID)((index + 1) % MAX_ACTORS);
    return actor;
}

static void destroy_actor(GameActor* actor) {
    if (actor == NULL)
        return;

    const ActorType type = actor->type;
    if (type <= ACT_NULL || type >= ACT_SIZE)
        WARN("Destroying invalid actor type %u", type);
    else
        ACTOR_CALL(actor, cleanup);

    // Unlink cell
    if (actor->cell >= 0 && actor->cell < GRID_SIZE) {
        GameActor* neighbor = get_actor(actor->previous_cell);
        if (neighbor != NULL)
            neighbor->next_cell = actor->next_cell;

        neighbor = get_actor(actor->next_cell);
        if (neighbor != NULL)
            neighbor->previous_cell = actor->previous_cell;

        if (game_state->grid[actor->cell] == actor->id)
            game_state->grid[actor->cell] = actor->previous_cell;

        actor->previous_cell = actor->next_cell = NULL_ACTOR;
        actor->cell = NULL_CELL;
    }

    // Unlink list
    GameActor* neighbor = get_actor(actor->previous);
    if (neighbor != NULL)
        neighbor->next = actor->next;

    neighbor = get_actor(actor->next);
    if (neighbor != NULL)
        neighbor->previous = actor->previous;

    if (game_state->live_actors == actor->id)
        game_state->live_actors = actor->previous;

    actor->previous = actor->next = NULL_ACTOR;

    actor->id = NULL_ACTOR;
    actor->type = ACT_NULL;
}

GameActor* get_actor(ActorID id) {
    if (id < 0 || id >= MAX_ACTORS)
        return NULL;

    GameActor* actor = &game_state->actors[id];
    return (actor->id < 0 || actor->id >= MAX_ACTORS) ? NULL : actor;
}

void move_actor(GameActor* actor, const FVec2 pos) {
    if (actor == NULL)
        return;

    actor->pos = pos;
    Sint32 cx = actor->pos.x / CELL_SIZE, cy = actor->pos.y / CELL_SIZE;
    cx = SDL_clamp(cx, 0, MAX_CELLS - 1);
    cy = SDL_clamp(cy, 0, MAX_CELLS - 1);

    const Sint32 cell = cx + (cy * MAX_CELLS);
    if (cell == actor->cell)
        return;

    // Unlink old cell
    if (actor->cell >= 0 && actor->cell < GRID_SIZE) {
        GameActor* neighbor = get_actor(actor->previous_cell);
        if (neighbor != NULL)
            neighbor->next_cell = actor->next_cell;

        neighbor = get_actor(actor->next_cell);
        if (neighbor != NULL)
            neighbor->previous_cell = actor->previous_cell;

        if (game_state->grid[actor->cell] == actor->id)
            game_state->grid[actor->cell] = actor->previous_cell;
    }

    // Link new cell
    actor->cell = cell;
    actor->previous_cell = game_state->grid[cell];
    actor->next_cell = NULL_ACTOR;

    GameActor* first = get_actor(game_state->grid[cell]);
    if (first != NULL)
        first->next_cell = actor->id;

    game_state->grid[cell] = actor->id;
}

Bool in_any_view(const FVec2 pos, Fixed edge, Bool ignore_top) {
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const Fixed cx = Fclamp(player->pos.x, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
                        player->bounds.end.x - F_HALF_SCREEN_WIDTH),
                    cy = Fclamp(player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT,
                        player->bounds.end.y - F_HALF_SCREEN_HEIGHT);
        const FRect cbox = {
            {cx - F_HALF_SCREEN_WIDTH + edge, cy - F_HALF_SCREEN_HEIGHT + edge},
            {cx + F_HALF_SCREEN_WIDTH - edge, cy + F_HALF_SCREEN_HEIGHT - edge}
        };
        if (pos.x < cbox.end.x && pos.x > cbox.start.x && pos.y < cbox.end.y && (ignore_top || pos.y > cbox.start.y))
            return TRUE;
    }

    return FALSE;
}

void collide_actor(GameActor* actor) {
    if (actor == NULL)
        return;

    const FRect abox = Radd(actor->box, actor->pos);
    Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
    Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
    cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
    cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
    cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
    cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

    for (Sint32 cx = cx1; cx <= cx2; cx++) {
        for (Sint32 cy = cy1; cy <= cy2; cy++) {
            for (GameActor* other = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]); other != NULL;
                other = get_actor(other->previous_cell))
            {
                if (actor == other || ANY_FLAG(other, FLG_DESTROY) || !Rcollide(abox, Radd(other->box, other->pos)))
                    continue;

                ACTOR_CALL(other, collide, actor);

                if (ANY_FLAG(actor, FLG_DESTROY))
                    return;
            }
        }
    }
}

Bool touching_solid(const FRect rect, SolidType types) {
    Sint32 cx1 = (rect.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (rect.start.y - CELL_SIZE) / CELL_SIZE;
    Sint32 cx2 = (rect.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (rect.end.y + CELL_SIZE) / CELL_SIZE;
    cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
    cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
    cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
    cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

    for (Sint32 cx = cx1; cx <= cx2; cx++) {
        for (Sint32 cy = cy1; cy <= cy2; cy++) {
            for (GameActor* actor = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]); actor != NULL;
                actor = get_actor(actor->previous_cell))
            {
                if (ACTOR_IS_SOLID(actor, types) && actor->type != ACT_SOLID_SLOPE
                    && Rcollide(rect, Radd(actor->box, actor->pos)))
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

// NOLINTBEGIN(misc-no-recursion)
void displace_actor(GameActor* actor, Fixed climb, Bool unstuck) {
    if (actor == NULL)
        return;

    if (unstuck && touching_solid(Radd(actor->box, actor->pos), SOL_SOLID)) {
        Fixed shift = ANY_FLAG(actor, FLG_X_FLIP) ? Fx1 : -Fx1;
        if (ANY_FLAG(actor, FLG_X_FLIP)) {
            if (touching_solid(BOX_OUTLINE_RIGHT(actor), SOL_SOLID)
                && !touching_solid(BOX_OUTLINE_LEFT(actor), SOL_SOLID))
            {
                shift = -shift;
            }
        } else if (touching_solid(BOX_OUTLINE_LEFT(actor), SOL_SOLID)
                   && !touching_solid(BOX_OUTLINE_RIGHT(actor), SOL_SOLID))
        {
            shift = -shift;
        }

        move_actor(actor, Vadd(actor->pos, (FVec2){shift, Fx0}));
        VAL(actor, X_SPEED) = VAL(actor, Y_SPEED) = Fx0;
        VAL(actor, X_TOUCH) = (shift >= Fx0) ? -1 : 1;
        VAL(actor, Y_TOUCH) = 1;

        return;
    }

    const GameActor* platform = get_actor(VAL(actor, PLATFORM));
    if (platform != NULL) {
        VAL(actor, PLATFORM) = NULL_ACTOR;

        const FVec2 avel = (FVec2){VAL(actor, X_SPEED), VAL(actor, Y_SPEED)};
        const FVec2 pvel = Vsub(platform->pos, platform->last_pos);

        VAL(actor, X_SPEED) = pvel.x;
        VAL(actor, Y_SPEED) = pvel.y;
        displace_actor(actor, Fx0, FALSE);
        VAL(actor, X_SPEED) = avel.x;
        VAL(actor, Y_SPEED) = avel.y;

        if (VAL(actor, PLATFORM) == platform->id) {
            if (!Rcollide(Radd(Radd(actor->box, actor->pos), (FVec2){pvel.x, pvel.y + Fx1}),
                    Radd(platform->box, platform->pos)))
            {
                VAL(actor, PLATFORM) = NULL_ACTOR;
            }
        }
    }

    VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0;
    FVec2 npos = actor->pos;

    // Horizontal collision
    if (VAL(actor, X_SPEED) != Fx0) {
        npos.x += VAL(actor, X_SPEED);
        const FRect abox = Radd(actor->box, npos);
        Bool climbed = FALSE, stop = FALSE;

        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        const Bool right = VAL(actor, X_SPEED) > Fx0;
        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                for (GameActor* displacer = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]); displacer != NULL;
                    displacer = get_actor(displacer->previous_cell))
                {
                    if (actor == displacer || !ACTOR_IS_SOLID(displacer, SOL_SOLID)
                        || displacer->type == ACT_SOLID_SLOPE || !Rcollide(abox, Radd(displacer->box, displacer->pos)))
                    {
                        continue;
                    }

                    if (VAL(actor, Y_SPEED) >= Fx0
                        && (npos.y + actor->box.end.y - climb) < (displacer->pos.y + displacer->box.start.y))
                    {
                        const Fixed step = displacer->pos.y + displacer->box.start.y - actor->box.end.y;
                        const FRect solid = Radd(Radd(actor->box, npos), (FVec2){right ? Fx1 : -Fx1, Fx0});

                        if (!touching_solid(solid, SOL_SOLID)) {
                            npos.y = step;
                            VAL(actor, Y_SPEED) = Fx0;
                            VAL(actor, Y_TOUCH) = 1;
                            climbed = TRUE;

                            goto da_climbed;
                        }

                        continue;
                    }

                    if (right) {
                        ACTOR_CALL(displacer, on_left, actor);
                        npos.x = Fmin(npos.x, displacer->pos.x + displacer->box.start.x - actor->box.end.x);
                        stop |= VAL(actor, X_SPEED) >= Fx0;
                    } else {
                        ACTOR_CALL(displacer, on_right, actor);
                        npos.x = Fmax(npos.x, displacer->pos.x + displacer->box.end.x - actor->box.start.x);
                        stop |= VAL(actor, X_SPEED) <= Fx0;
                    }

                    climbed = FALSE;
                }
            }
        }

        if (stop) {
            if (!climbed)
                VAL(actor, X_TOUCH) = right ? 1 : -1;
            VAL(actor, X_SPEED) = Fx0;
        }
    }

da_climbed:
    // Vertical collision
    if (VAL(actor, Y_SPEED) != Fx0) {
        npos.y += VAL(actor, Y_SPEED);
        const FRect abox = Radd(actor->box, npos);
        Bool stop = FALSE;

        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                for (GameActor* displacer = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]); displacer != NULL;
                    displacer = get_actor(displacer->previous_cell))
                {
                    if (actor == displacer || !Rcollide(abox, Radd(displacer->box, displacer->pos)))
                        continue;

                    if (displacer->type == ACT_SOLID_SLOPE) {
                        if (VAL(actor, Y_SPEED) < Fx0) {
                            if (ANY_FLAG(displacer, FLG_Y_FLIP) || npos.y < (displacer->pos.y + displacer->box.end.y))
                                continue;

                            ACTOR_CALL(displacer, on_bottom, actor);
                            npos.y = Fmax(npos.y, displacer->pos.y + displacer->box.end.y - actor->box.start.y);
                            stop |= VAL(actor, Y_SPEED) <= Fx0;
                        } else {
                            const Fixed width = Fabs(displacer->box.end.x - displacer->box.start.x);
                            if (width == Fx0)
                                continue;

                            const Bool side = ANY_FLAG(displacer, FLG_X_FLIP);
                            const Fixed sa = displacer->pos.y + (side ? displacer->box.start.y : displacer->box.end.y),
                                        sb = displacer->pos.y + (side ? displacer->box.end.y : displacer->box.start.y),
                                        ax = npos.x + (side ? actor->box.start.x : actor->box.end.x),
                                        sx = displacer->pos.x + displacer->box.start.x;

                            const Fixed slope = Flerp(sa, sb, Fclamp(Fdiv(ax - sx, width), Fx0, Fx1));
                            if ((npos.y + actor->box.end.y + Fabs(VAL(actor, X_SPEED))) >= slope) {
                                npos.y = slope - actor->box.end.y;
                                stop = TRUE;
                            }
                        }

                        continue;
                    }

                    if (VAL(actor, Y_SPEED) < Fx0) {
                        if (!ACTOR_IS_SOLID(displacer, SOL_SOLID))
                            continue;

                        ACTOR_CALL(displacer, on_bottom, actor);
                        npos.y = Fmax(npos.y, displacer->pos.y + displacer->box.end.y - actor->box.start.y);
                        stop |= VAL(actor, Y_SPEED) <= Fx0;
                    } else {
                        const SolidType solid = ACTOR_GET_SOLID(displacer);
                        if (!(solid & SOL_ALL)
                            || ((solid & SOL_TOP)
                                && (npos.y + actor->box.end.y - VAL(actor, Y_SPEED))
                                       > (displacer->pos.y + displacer->box.start.y + climb)))
                        {
                            continue;
                        }

                        ACTOR_CALL(displacer, on_top, actor);
                        npos.y = Fmin(npos.y, displacer->pos.y + displacer->box.start.y - actor->box.end.y);
                        stop |= VAL(actor, Y_SPEED) >= Fx0;
                    }
                }
            }
        }

        if (stop) {
            VAL(actor, Y_TOUCH) = (VAL(actor, Y_SPEED) < Fx0) ? -1 : 1;
            VAL(actor, Y_SPEED) = Fx0;
        }
    }

    move_actor(actor, npos);
}
// NOLINTEND(misc-no-recursion)

void draw_actor(const GameActor* actor, const char* sprite, Bool antijitter) {
    if (actor == NULL || !ANY_FLAG(actor, FLG_VISIBLE))
        return;

    const FVec2 ipos = get_interp(actor);
    if (antijitter) {
        const FVec2 cpos = videostate()->camera.pos;
        const Sint32 ax = Fx2Int(ipos.x - Ffrac(cpos.x)), ay = Fx2Int(ipos.y - Ffrac(cpos.y));
        batch_pos(B_XYZ(ax, ay, Fx2Float(actor->depth)));
    } else {
        const Sint32 ax = Fx2Int(ipos.x), ay = Fx2Int(ipos.y);
        batch_pos(B_XYZ(ax, ay, Fx2Float(actor->depth)));
    }
    batch_flip(B_FLIP(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
    batch_sprite(sprite);
}

/// Produces an exclusive random number.
///
/// Based on https://github.com/libsdl-org/SDL/blob/fe1918a47fb8b13a76a96fd1f07e1f3ff941a4e1/src/stdlib/SDL_random.c#L91
Sint32 rng(Sint32 n) {
    if (n <= 0)
        return 0;

    game_state->seed = (game_state->seed * 4280078389ULL) + 5ULL;

    const Uint64 val = (Uint64)((Uint32)(game_state->seed >> 16)) * (Uint32)n;
    return (Sint32)(val >> 32);
}

// ======
// INTERP
// ======

#define BAD_ACTOR(actor) ((actor) == NULL || (actor)->id < 0 || (actor)->id >= MAX_ACTORS)

const FVec2 get_interp(const GameActor* actor) {
    return (BAD_ACTOR(actor)) ? (FVec2){0} : interp_actors[actor->id].pos;
}

void skip_interp(const GameActor* actor) {
    if (BAD_ACTOR(actor))
        return;

    InterpActor* iactor = &interp_actors[actor->id];
    iactor->type = actor->type;
    iactor->from = iactor->to = iactor->pos = actor->pos;
}

void align_interp(const GameActor* actor, const GameActor* from) {
    if (BAD_ACTOR(actor) || BAD_ACTOR(from))
        return;

    InterpActor *iactor = &interp_actors[actor->id], *ifrom = &interp_actors[from->id];
    iactor->from = ifrom->from;
    iactor->to = ifrom->to;
    iactor->pos = ifrom->pos;
}

#undef BAD_ACTOR
