#include <SDL3/SDL_timer.h>

#include "K_audio.h"
#include "K_cmd.h"
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
#include "actors/K_enemies.h"
#include "actors/K_goal.h"
#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_projectiles.h"
#include "actors/K_screen.h"
#include "actors/K_warp.h"

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

#define ACTOR_IS_SOLID(act, types) ((ACTOR_GET_SOLID(act) & (types)) != 0)

typedef struct {
    GameState game;
    AudioState audio;
} SaveState;

typedef struct {
    Fixed from, to, current;
} InterpPlayer;

typedef struct {
    ActorType type;
    FVec2 from, to, current;
} InterpActor;

typedef struct {
    InterpPlayer players[MAX_PLAYERS];
    InterpActor actors[MAX_ACTORS];
} InterpState;

typedef struct {
    Bool is_actor;
    const void* ptr;
} SortedItem;

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
                [PF_DUCK] = "characters/mario/small/idle",
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
                [PF_DUCK] = "characters/luigi/small/idle",
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

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;

GameContext queue_game_context = {0};
static GameContext game_context = {0};

static LevelInfo* level_info = NULL;
static GameState* game_state = NULL;
static InterpState* interp_state = NULL;

static Uint8 boot_state = 0;
static char boot_reason[256] = "";

void game_init() {
    extern void POPULATE_ACTORS_TABLE();
    POPULATE_ACTORS_TABLE();

    recalculate_game_hash();
}

void recalculate_game_hash() {
    game_hash = 0;

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
        return LFMT("value.none");
    case POW_SUPER_MUSHROOM:
        return LFMT("value.super_mushroom");
    case POW_FIRE_FLOWER:
        return LFMT("value.fire_flower");
    case POW_BEETROOT:
        return LFMT("value.beetroot");
    case POW_GREEN_LUI:
        return LFMT("value.green_lui");
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
    gctx.seed = SDL_GetTicksNS();

    if (wctx != NULL) {
        gctx.num_players = wctx->num_players;
        for (PlayerID i = 0; i < (PlayerID)SDL_arraysize(gctx.players); i++) {
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
    if (level == NULL) {
        if (get_screen() != SCR_MENU) {
            bail_from_game();
            set_screen(SCR_MENU, NULL, 0);
        }

        WTF("Invalid level key %" SDL_PRIu64, ctx->level);
        return;
    }

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
        player->lives = DEFAULT_LIVES;
        player->track = 255;
    }

    game_state->live_actors = NULL_ACTOR;
    for (ActorID i = 0; i < MAX_ACTORS; i++) {
        GameActor* actor = &game_state->actors[i];
        actor->id = actor->previous = actor->next = actor->previous_cell = actor->next_cell = NULL_ACTOR;
        actor->player = NULL_PLAYER;
        actor->platform = NULL_ACTOR;
    }
    for (Sint32 i = 0; i < GRID_SIZE; i++)
        game_state->grid[i] = NULL_ACTOR;

    game_state->spawn = game_state->checkpoint = game_state->autoscroll = game_state->water = NULL_ACTOR;

    game_state->clock = -1;

    game_state->sequence.activator = NULL_PLAYER;

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

    yyjson_val* jval = yyjson_obj_get(root, "label");
    if (yyjson_is_str(jval)) {
        const char* label = SDL_strdup(yyjson_get_str(jval));
        EXPECT(label, "Failed to allocate level \"%s\" label", level->name);
        if ((label[0] != '@' && label[0] != '$') || label[0] == '%') {
            // FIXME: This only loads the sprite for the current language.
            //        The label sprite will "disappear" when changing to another language.
            load_sprite(LFMT(label), AKL_NEVER);
        }

        level_info->strings[GSTR_LABEL] = label;
    }

    jval = yyjson_obj_get(root, "tracks");
    for (Uint64 i = 0, n = yyjson_arr_size(jval); i < n && i < MAX_GAME_TRACKS; i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jval2))
            continue;

        const char* otrack = yyjson_get_str(yyjson_obj_get(jval2, "track"));
        if (otrack == NULL)
            continue;

        const char* track = SDL_strdup(otrack);
        EXPECT(track, "Failed to allocate level \"%s\" track %" SDL_PRIu64, level->name, i + 1);
        load_track(track, AKL_NEVER);

        level_info->strings[GSTR_TRACK_START + i] = track;
        level_info->track_offsets[i] = yyjson_get_uint(yyjson_obj_get(jval2, "offset"));
    }

    jval = yyjson_obj_get(root, "warps");
    for (Uint64 i = 0, n = yyjson_arr_size(jval); i < n && i < MAX_GAME_WARPS; i++)
        level_info->warps[i] = StHashStr(yyjson_get_str(yyjson_arr_get(jval, i)));

    jval = yyjson_obj_get(root, "secrets");
    for (Uint64 i = 0, n = yyjson_arr_size(jval); i < n && i < MAX_GAME_SECRETS; i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (!yyjson_is_str(jval2))
            continue;

        const char* track = SDL_strdup(yyjson_get_str(jval2));
        EXPECT(track, "Failed to allocate level \"%s\" secret %" SDL_PRIu64, level->name, i + 1);
        // TODO: Do something with secret strings
        level_info->strings[GSTR_SECRET_START + i] = track;
    }

    jval = yyjson_obj_get(root, "size");
    if (yyjson_is_arr(jval)) {
        level_info->size.x = Int2Fx(yyjson_get_uint(yyjson_arr_get(jval, 0)));
        level_info->size.y = Int2Fx(yyjson_get_uint(yyjson_arr_get(jval, 1)));
    }

    jval = yyjson_obj_get(root, "bounds");
    if (yyjson_is_arr(jval)) {
        level_info->bounds.start.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 0)));
        level_info->bounds.start.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 1)));
        level_info->bounds.end.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 2)));
        level_info->bounds.end.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 3)));
    }

    jval = yyjson_obj_get(root, "time");
    if (yyjson_is_int(jval))
        game_state->clock = (Sint16)yyjson_get_sint(jval);

    jval = yyjson_obj_get(root, "bowser_bounds");
    if (yyjson_is_arr(jval)) {
        level_info->bowser_bounds.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 0)));
        level_info->bowser_bounds.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jval, 1)));
    }

    if (yyjson_get_bool(yyjson_obj_get(root, "hardcore")))
        game_state->flags |= GF_HARDCORE;
    if (yyjson_get_bool(yyjson_obj_get(root, "lost_map")))
        game_state->flags |= GF_LOST_MAP;
    if (yyjson_get_bool(yyjson_obj_get(root, "funny_tanks")))
        game_state->flags |= GF_FUNNY_TANKS;
    if (yyjson_get_bool(yyjson_obj_get(root, "ambush")))
        set_sequence(GS_AMBUSH, NULL, 0);

    read_tilemap(videostate()->tilemap, yyjson_obj_get(root, "backdrops"));

    jval = yyjson_obj_get(root, "collisions");
    const Uint64 jnum = yyjson_arr_size(jval);
    level_info->num_collisions = SDL_min(jnum, SDL_MAX_UINT8);
    level_info->collisions = SDL_calloc(level_info->num_collisions, sizeof(*level_info->collisions));
    EXPECT(level_info->collisions, "Failed to allocate level \"%s\" collisions", level->name);
    for (Uint8 i = 0; i < level_info->num_collisions; i++) {
        yyjson_val* jcmap = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jcmap))
            continue;

        CollisionMap* cmap = &level_info->collisions[i];

        yyjson_val* jcmval = yyjson_obj_get(jcmap, "pos");
        cmap->bounds.start.x = Int2Fx(yyjson_get_sint(yyjson_arr_get(jcmval, 0)));
        cmap->bounds.start.y = Int2Fx(yyjson_get_sint(yyjson_arr_get(jcmval, 1)));

        jcmval = yyjson_obj_get(jcmap, "size");
        cmap->size[0] = yyjson_get_uint(yyjson_arr_get(jcmval, 0));
        cmap->size[1] = yyjson_get_uint(yyjson_arr_get(jcmval, 1));

        jcmval = yyjson_obj_get(jcmap, "cell_size");
        cmap->cell_size.x = Int2Fx(yyjson_get_uint(yyjson_arr_get(jcmval, 0)));
        cmap->cell_size.y = Int2Fx(yyjson_get_uint(yyjson_arr_get(jcmval, 1)));

        cmap->bounds.end.x = cmap->bounds.start.x + (Fixed)cmap->size[0] * cmap->cell_size.x;
        cmap->bounds.end.y = cmap->bounds.start.y + (Fixed)cmap->size[1] * cmap->cell_size.y;

        cmap->grid = SDL_calloc((Uint64)cmap->size[0] * (Uint64)cmap->size[1], sizeof(*cmap->grid));
        EXPECT(cmap->grid, "Failed to allocate %ux%u collision grid for level \"%s\"", cmap->size[0], cmap->size[1],
            level->name);

        jcmval = yyjson_obj_get(jcmap, "cells");
        for (Uint64 j = 0, n = yyjson_arr_size(jcmval); j < n; j++) {
            yyjson_val* jccell = yyjson_arr_get(jcmval, j);
            const Uint64 cx = yyjson_get_uint(yyjson_arr_get(jccell, 0)) % cmap->size[0],
                         cy = yyjson_get_uint(yyjson_arr_get(jccell, 1)) % cmap->size[1];
            cmap->grid[cx + (cy * cmap->size[0])] = yyjson_get_uint(yyjson_arr_get(jccell, 2));
        }
    }

    jval = yyjson_obj_get(root, "actors");
    for (Uint64 i = 0, n = yyjson_arr_size(jval); i < n && i < MAX_ACTORS; i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jval2))
            continue;

        if (yyjson_get_bool(yyjson_obj_get(jval2, "once")) && (game_context.flags & GF_RESTARTED)
            || (yyjson_get_bool(yyjson_obj_get(jval2, "singleplayer")) && game_context.num_players > 1)
            || (yyjson_get_bool(yyjson_obj_get(jval2, "multiplayer")) && game_context.num_players <= 1))
        {
            // Keep actor ID order so that checkpoints are valid between restarts.
            game_state->next_actor = (ActorID)((game_state->next_actor + 1) % MAX_ACTORS);
            continue;
        }

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

        yyjson_val* jvel = yyjson_obj_get(jval2, "vel");
        if (yyjson_is_arr(jvel)) {
            actor->vel.x = (Fixed)yyjson_get_sint(yyjson_arr_get(jvel, 0));
            actor->vel.y = (Fixed)yyjson_get_sint(yyjson_arr_get(jvel, 1));
        }

        yyjson_val* jvalues = yyjson_obj_get(jval2, "values");
        for (Uint64 j = 0, n = yyjson_arr_size(jvalues); j < n && j < MAX_VALUES; j++) {
            yyjson_val* jvalue = yyjson_arr_get(jvalues, j);

            const ActorValue idx = (ActorValue)yyjson_get_uint(yyjson_arr_get(jvalue, 0));
            if (idx < 0 || idx >= MAX_VALUES) {
                WTF("Invalid value index %i for actor %" SDL_PRIu64 " type %u", idx, i, type);
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

    interp_state = SDL_calloc(1, sizeof(*interp_state));
    EXPECT(interp_state, "Failed to allocate interpolation state");

    // Load assets
    load_sprite("ui/bezel_l", AKL_NEVER);
    load_sprite("ui/bezel_r", AKL_NEVER);

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
    game_surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, FALSE);

    // Initial game state
    game_state->seed = game_context.seed;
    INFO("Game seed is %" SDL_PRIu64, game_state->seed);

    game_state->flags |= game_context.flags;

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

        for (Uint8 i = 0; i < level_info->num_collisions; i++)
            SDL_free(level_info->collisions[i].grid);
        SDL_free(level_info->collisions);

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
    for (Uint64 i = 0; i < sizeof(*game_state); i++)
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
        player->input = in_blocking_sequence() ? 0 : inputs[i];

        if ((game_state->flags & GF_1UP) && (game_state->time % 25) == 0)
            give_points(NULL, player, -1);
    }

    GameActor* actor = get_actor(game_state->live_actors);
    while (actor != NULL) {
        actor->last_pos = actor->pos;

        if (actor->sprout > 0)
            --actor->sprout;

        if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE))
            ACTOR_CALL(actor, pre_tick);

        GameActor* next = get_actor(actor->previous);
        if (ANY_FLAG(actor, FLG_DESTROY))
            destroy_actor(actor);
        actor = next;
    }

#define TICK_LOOP(ticker)                                                                                              \
    actor = get_actor(game_state->live_actors);                                                                        \
    while (actor != NULL) {                                                                                            \
        if (!ANY_FLAG(actor, FLG_DESTROY | FLG_FREEZE))                                                                \
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

    GameSequence* sequence = get_sequence();
    switch (sequence->type) {
    default:
        break;

    case GS_NONE: {
        if (game_state->clock <= 0 || game_state->time <= 0 || (game_state->time % 25) > 0)
            break;

        Bool all_inactive = TRUE;
        for (PlayerID i = 0; i < game_context.num_players; i++) {
            const GamePlayer* player = get_player(i);
            if (player == NULL)
                continue;

            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER && get_actor(VAL(pawn, PLAYER_WARP)) == NULL
                && !ANY_FLAG(pawn, FLG_PLAYER_WARP_OUT))
            {
                all_inactive = FALSE;
                break;
            }
        }

        if (all_inactive)
            break;

        --game_state->clock;

        if (game_state->clock <= 100 && !(game_state->flags & GF_HURRY)) {
            game_state->flags |= GF_HURRY;

            // !!! CLIENT-SIDE !!!
            videostate()->hurry = 1;
            // !!! CLIENT-SIDE !!!

            play_state_sound("hurry", 0, NULL);
        }

        if (game_state->clock <= 0) {
            for (PlayerID i = 0; i < game_context.num_players; i++) {
                GamePlayer* player = get_player(i);
                if (player == NULL)
                    continue;

                kill_player(get_actor(player->actor));
            }
        }

        break;
    }

    case GS_LOSE: {
        switch (sequence->time++) {
        default:
            break;

        case 0: {
            for (PlayerID i = 0; i < game_context.num_players; i++) {
                GamePlayer* player = get_player(i);
                if (player == NULL)
                    continue;

                if (player->id == sequence->activator) {
                    set_view_player(player);
                    continue;
                }

                GameActor* actor = get_actor(player->actor);
                if (actor != NULL)
                    FLAG_ON(actor, FLG_DESTROY);
            }

            GameActor* autoscroll = get_actor(game_state->autoscroll);
            if (autoscroll != NULL && !ANY_FLAG(autoscroll, FLG_SCROLL_BOWSER | FLG_SCROLL_TANKS))
                autoscroll->vel.x = autoscroll->vel.y = Fx0;

            if (game_state->flags & GF_LOST_MAP)
                play_state_track(sequence->activator, "smw/lose2", 0, 0);
            else if (game_state->flags & GF_HARDCORE)
                play_state_track(sequence->activator, "smw/lose_hardcore", 0, 0);
            else
                play_state_track(sequence->activator, "smw/lose", 0, 0);

            break;
        }

        case 201: {
            if (game_context.num_players <= 1) {
                GamePlayer* player = get_player(sequence->activator);
                if (player != NULL)
                    --player->lives;
            }

            if (!all_players_dead())
                game_state->flags |= GF_END;

            break;
        }

        case 211: {
            play_state_track(sequence->activator, "smb/game_over", 0, 0);
            break;
        }

        case 511: {
            game_state->flags |= GF_END;
            break;
        }
        }

        break;
    }

    case GS_WIN: {
        if (sequence->time < 400)
            ++sequence->time;

        if (sequence->time >= 400 || (game_state->flags & GF_LOST_MAP)) {
            if (game_state->clock > 0) {
                GamePlayer* player = get_player(sequence->activator);

                --game_state->clock;
                if (player != NULL)
                    player->score += 10;

                if (game_state->clock >= 10) {
                    game_state->clock -= 10;
                    if (player != NULL)
                        player->score += 100;
                }

                if ((game_state->time % 5) == 0) {
                    if ((game_state->flags & GF_LOST_MAP) && game_state->clock > 0) {
                        --game_state->clock;
                        if (player != NULL)
                            ++player->score;
                    }

                    play_state_sound("tick", 0, NULL);
                }
            } else if (sequence->state < 50) {
                ++sequence->state;
            }
        }

        if (sequence->state >= 50 && (!(game_state->flags & GF_LOST_MAP) || sequence->time >= 140))
            game_state->flags |= GF_END;

        break;
    }

    case GS_WARP: {
        ++sequence->time;
        if ((sequence->state > 0 && sequence->time > 200) || (sequence->state <= 0 && sequence->time > 60))
            game_state->flags |= GF_END;

        break;
    }

    case GS_BOWSER_END: {
        if (sequence->time == 0) {
            fade_state_track(ALL_TRACKS, 0.f, 100.f);
            ++sequence->time;
        }

        if ((sequence->time == 1 && get_num_actors(ACT_BOWSER_DEAD) <= 0) || sequence->time > 1) {
            if (++sequence->time > 11)
                win_player(get_player(sequence->activator));
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
    SDL_free(interp_state);
    interp_state = NULL;
    local_player = view_player = NULL_PLAYER;
}

void tick_game() {
    if (game_session == NULL)
        return;

    if (get_replay_state() == RPS_PLAYING) {
        const GameInput* input = read_replay();
        if (input == NULL) {
            boot_to_menu(LFMT("message.replay_ended"));
            return;
        }

        for (PlayerID i = 0; i < game_context.num_players; i++)
            gekko_add_local_input(game_session, i, (void*)&input[i]);
    } else {
        // Since this isn't a replay, we can freely change levels/worlds.
        if (game_state->flags & GF_END) {
            if (!is_leader())
                return;

            const GameSequence* sequence = get_sequence();
            switch (sequence->type) {
            default:
                return;

            case GS_LOSE: {
                GameContext ctx = game_context;
                ctx.seed = SDL_GetTicksNS();
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

            case GS_WIN: {
                WorldContext ctx = *worldcontext();
                ++ctx.level;
                ctx.winner = sequence->activator;
                for (PlayerID i = 0; i < ctx.num_players; i++) {
                    ctx.players[i].lives = game_state->players[i].lives;
                    ctx.players[i].coins = game_state->players[i].coins;
                    ctx.players[i].score = game_state->players[i].score;
                    ctx.players[i].powerup = game_state->players[i].powerup;
                }

                jump_to_world(&ctx, FALSE);
                return;
            }

            case GS_WARP: {
                const GamePlayer* player = get_player(sequence->activator);
                if (player == NULL)
                    return;

                const GameActor* pawn = get_actor(player->actor);
                if (pawn == NULL || pawn->type != ACT_PLAYER)
                    return;

                const GameActor* warp = get_actor(VAL(pawn, PLAYER_WARP));
                if (warp == NULL || VAL(warp, WARP_ID) < 0 || VAL(warp, WARP_ID) >= MAX_GAME_WARPS)
                    return;

                if (ANY_FLAG(warp, FLG_WARP_WORLD)) {
                    WorldContext ctx = *worldcontext();
                    ctx.world = level_info->warps[VAL(warp, WARP_ID)];
                    ctx.level = 0;
                    ctx.winner = sequence->activator;
                    for (PlayerID i = 0; i < ctx.num_players; i++) {
                        ctx.players[i].lives = game_state->players[i].lives;
                        ctx.players[i].coins = game_state->players[i].coins;
                        ctx.players[i].score = game_state->players[i].score;
                        ctx.players[i].powerup = game_state->players[i].powerup;
                    }

                    jump_to_world(&ctx, FALSE);
                } else if (ANY_FLAG(warp, FLG_WARP_LEVEL)) {
                    GameContext ctx = init_game_context(worldcontext(), level_info->warps[VAL(warp, WARP_ID)]);
                    for (PlayerID i = 0; i < ctx.num_players; i++) {
                        ctx.players[i].lives = game_state->players[i].lives;
                        ctx.players[i].coins = game_state->players[i].coins;
                        ctx.players[i].score = game_state->players[i].score;
                        ctx.players[i].powerup = game_state->players[i].powerup;
                    }

                    jump_to_game(&ctx, FALSE);
                }

                return;
            }
            }
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
                LFMT("message.player_desynced", 's', get_peer_name(player_to_peer((PlayerID)desync.remote_handle))));

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

            boot_to_menu(LFMT("message.player_disconnected", 's', get_peer_name(player_to_peer((PlayerID)dc.handle))));
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
                boot_to_menu(LFMT("message.replay_desynced"));
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
        for (PlayerID i = 0; i < game_context.num_players; i++) {
            InterpPlayer* iplayer = &interp_state->players[i];
            iplayer->from = iplayer->to = iplayer->current = game_state->players[i].xscroll;
        }

        FOR_EACH_ACTOR (actor) {
            InterpActor* iactor = &interp_state->actors[actor->id];
            iactor->type = actor->type;
            iactor->from = iactor->to = iactor->current = actor->pos;
        }

        return;
    }

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        InterpPlayer* iplayer = &interp_state->players[i];
        iplayer->from = iplayer->to;
        iplayer->to = game_state->players[i].xscroll;
    }

    FOR_EACH_ACTOR (actor) {
        InterpActor* iactor = &interp_state->actors[actor->id];
        if (iactor->type == actor->type) {
            iactor->from = iactor->to;
            iactor->to = actor->pos;
        } else {
            iactor->type = actor->type;
            iactor->from = iactor->to = iactor->current = actor->pos;
        }
    }
}

void interp_game() {
    const int fps = get_framerate();
    if (fps > 0 && fps <= TICKRATE)
        return;

    const Fixed t = Float2Fx(pendingticks());

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        InterpPlayer* iplayer = &interp_state->players[i];
        iplayer->current = Flerp(iplayer->from, iplayer->to, t);
    }

    const GameActor* actor = NULL;
    FOR_EACH_ACTOR (actor) {
        InterpActor* iactor = &interp_state->actors[actor->id];
        iactor->current = Vlerp(iactor->from, iactor->to, t);
    }
}

static void draw_hud() {
    const GamePlayer* player = get_player(view_player);
    if (player == NULL)
        return;

    VideoState* video_state = videostate();
    video_state->bowser.health = 0;

    const GameActor* actor = NULL;
    FOR_EACH_ACTOR (actor) { ACTOR_CALL(actor, draw_hud); }

    batch_reset();

    if (video_state->bowser.active) {
        if (video_state->bowser.y < 60.f) {
            video_state->bowser.y += deltaticks() * 4.f;
            if (video_state->bowser.y >= 60.f)
                video_state->bowser.y = 60.f;
        }

        batch_pos(B_F3_XY(SCREEN_WIDTH - 75.f, video_state->bowser.y));
        batch_sprite("ui/bowser");
        for (Uint8 i = 0; i < video_state->bowser.health; i++) {
            batch_pos(B_F3_XY(SCREEN_WIDTH - 75.f - ((float)i * 9.f), video_state->bowser.y));
            batch_sprite("ui/bowser/bar");
        }
    }

    batch_pos(B_F3_XY(32.f, 16.f));
    const char* cname = get_character_name(game_context.players[player->id].character);
    batch_string("hud", 16.f, fmt("%s × %i", cname, SDL_max(player->lives, 0)));
    batch_pos(B_F3_XY(32.f + string_width("hud", 16.f, fmt("%s × 0", cname)), 34.f));
    batch_align(B_ALIGN_TOP_RIGHT);
    batch_string("hud", 16.f, fmt("%u", player->score));

    batch_pos(B_F3_XY(224.f, 34.f));
    batch_sprite(fmt("ui/coins/%i", ((game_state->time * 4) / 25) % 3));
    batch_pos(B_F3_XY(235.f, 34.f));
    batch_align(B_ALIGN_TOP_LEFT);
    batch_string("hud", 16.f, " ×");
    batch_pos(B_F3_XY(293.f, 34.f));
    batch_align(B_ALIGN_TOP_RIGHT);
    batch_string("hud", 16.f, fmt("%u", player->coins));

    const char* label = level_info->strings[GSTR_LABEL];
    if (label != NULL) {
        switch (label[0]) {
        default: {
            batch_pos(B_F3_XY(432.f, 16.f));
            batch_sprite(LFMT(label));
            break;
        }

        case '$': {
            batch_pos(B_F3_XY(432.f, 34.f));
            batch_align(B_ALIGN_CENTER);
            batch_string("hud", 16.f, LFMT(label + 1));
            break;
        }

        case '@': {
            batch_pos(B_F3_XY(432.f, 16.f));
            batch_align(B_ALIGN(FA_CENTER, FA_TOP));
            batch_string("hud", 16.f, LFMT("hud.world"));
            batch_pos(B_F3_XY(432.f, 34.f));
            batch_string("hud", 16.f, LFMT(label + 1));
            break;
        }

        case '%': {
            batch_pos(B_F3_XY(432.f, 16.f));
            batch_align(B_ALIGN(FA_CENTER, FA_TOP));
            batch_string("hud", 16.f, LFMT("hud.world"));
            batch_pos(B_F3_XY(432.f, 34.f));
            batch_sprite(LFMT(label + 1));
            break;
        }
        }
    }

    if (game_state->clock >= 0) {
        batch_pos(B_F3_XY(SCREEN_WIDTH - 32.f, 24.f));

        if (video_state->hurry > 0 && video_state->hurry <= 120) {
            const float hurry = (float)((video_state->hurry - 1) % 12);
            batch_scale(B_F2(
                1.f, (hurry < 6.f) ? (1.f - ((hurry / 6.f) * 0.375f)) : (0.625f + (((hurry - 6.f) / 6.f) * 0.375f))));
        }

        batch_align(B_ALIGN(FA_RIGHT, FA_MIDDLE));
        batch_string("hud", 16.f, LFMT("hud.time"));
        batch_pos(B_F3_XY(SCREEN_WIDTH - 32.f, 34.f));
        batch_scale(B_F2_1);
        batch_align(B_ALIGN_TOP_RIGHT);
        batch_string("hud", 16.f, fmt("%i", game_state->clock));
    }

    if (view_player != local_player) {
        const char* name = get_peer_name(player_to_peer(view_player));
        if (name != NULL) {
            batch_pos(B_F3_XY(32.f, 64.f));
            batch_align(B_ALIGN_TOP_LEFT);
            batch_string("main", 24.f, fmt("%s: %s", LFMT("hud.spectating"), name));
        }
    }

    const GameSequence* sequence = get_sequence();
    if (sequence->type == GS_LOSE && sequence->time >= 211) {
        batch_pos(B_F3_HALF_SCREEN);
        batch_align(B_ALIGN_CENTER);
        batch_string("hud", 16.f, LFMT("hud.game_over"));
    }
}

static void draw_game_state() {
    static mat4 oview = GLM_MAT4_IDENTITY_INIT, view = GLM_MAT4_IDENTITY_INIT;

    get_view_matrix(oview);

    batch_filter(FALSE);

    VideoState* video_state = videostate();
    VideoCamera* camera = &video_state->camera;

    const GameActor* autoscroll = get_actor(game_state->autoscroll);
    if (autoscroll == NULL) {
        const GamePlayer* player = get_player(view_player);
        if (player != NULL) {
            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER) {
                camera->pos = Vclamp(Vadd(get_interp(pawn), (FVec2){interp_state->players[player->id].current, Fx0}),
                    Vadd(player->bounds.start, F_HALF_SCREEN), Vsub(player->bounds.end, F_HALF_SCREEN));
            }
        }
    } else {
        camera->pos
            = Vadd(Vclamp(get_interp(autoscroll), level_info->bounds.start, Vsub(level_info->bounds.end, F_SCREEN)),
                F_HALF_SCREEN);
    }

    const Sint32 cx = Fx2Int(camera->pos.x), cy = Fx2Int(camera->pos.y);
    glm_look((vec3){(float)(cx - HALF_SCREEN_WIDTH), (float)(cy - HALF_SCREEN_HEIGHT), 0.f}, GLM_ZUP, GLM_YUP, view);
    set_view_matrix(view);
    apply_matrices();

    TinyPq sorter = {0};

    TileMap* tilemap = video_state->tilemap;
    tilemap_iterate_start(tilemap);
    const TileMapLayer* layer = NULL;
    while ((layer = tilemap_iterate_next(tilemap)))
        TinyPqInsert(&sorter, layer->depth, (const void*)&(SortedItem){FALSE, layer}, sizeof(SortedItem));

    const GameActor* actor = NULL;
    FOR_EACH_ACTOR (actor) {
        if (ANY_FLAG(actor, FLG_VISIBLE)) {
            TinyPqInsert(&sorter, (actor->sprout > 0) ? Fmax(actor->depth, Int2Fx(21)) : actor->depth,
                (const void*)&(SortedItem){TRUE, actor}, sizeof(SortedItem));
        }
    }

    TINY_PQ_FOREACH (&sorter, iter) {
        const SortedItem* sitem = iter.data;
        if (sitem->is_actor) {
            const GameActor* actor = sitem->ptr;
            ACTOR_CALL(actor, draw);
        } else {
            draw_tilemap_layer(sitem->ptr);
        }
    }

    FreeTinyPq(&sorter);

    set_view_matrix(oview);
    apply_matrices();

    draw_hud();

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

const char* get_game_secret(Uint8 sid) {
    return (sid < 0 || sid > MAX_GAME_SECRETS) ? NULL : level_info->strings[GSTR_SECRET_START + sid];
}

GameSequence* get_sequence() {
    return &game_state->sequence;
}

void set_sequence(GameSequenceType type, const GamePlayer* player, Sint16 state) {
    game_state->sequence.type = type;
    game_state->sequence.activator = (player == NULL) ? NULL_PLAYER : player->id;
    game_state->sequence.state = state;
    game_state->sequence.time = 0;
}

Bool in_blocking_sequence() {
    switch (get_sequence()->type) {
    default:
        return FALSE;
    case GS_LOSE:
    case GS_WIN:
    case GS_WARP:
        return TRUE;
    }

    return FALSE;
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
void set_view_player(const GamePlayer* player) {
    if (player == NULL || view_player == player->id)
        return;

    const GamePlayer* old_player = get_player(view_player);
    view_player = player->id;
}
/// !!! CLIENT-SIDE !!!

//
GamePlayer* get_player(PlayerID pid) {
    return (pid < 0 || pid >= MAX_PLAYERS || game_state->players[pid].id != pid) ? NULL : &game_state->players[pid];
}

GameActor* respawn_player(GamePlayer* player) {
    if (player == NULL || in_blocking_sequence())
        return NULL;

    if (player->lives < 0)
        goto rp_spectate;

    const GameActor* spawn = get_actor(game_state->autoscroll);
    if (spawn == NULL)
        spawn = get_actor(game_state->checkpoint);
    if (spawn == NULL)
        spawn = get_actor(game_state->spawn);
    if (spawn == NULL)
        goto rp_spectate;

    FVec2 spos = spawn->pos;
    switch (spawn->type) {
    default: {
        player->bounds = level_info->bounds;
        set_player_track(player, 0);
        break;
    }

    case ACT_CHECKPOINT: {
        spos = Vadd(spos, (FVec2){Int2Fx(53), Int2Fx(118)});

        const Fixed bx1 = VAL(spawn, CHECKPOINT_BOUNDS_X1), by1 = VAL(spawn, CHECKPOINT_BOUNDS_Y1),
                    bx2 = VAL(spawn, CHECKPOINT_BOUNDS_X2), by2 = VAL(spawn, CHECKPOINT_BOUNDS_Y2);
        player->bounds = (bx1 == bx2 && by1 == by2) ? level_info->bounds
                                                    : (FRect){
                                                          {bx1, by1},
                                                          {bx2, by2}
        };

        set_player_track(player, VAL(spawn, CHECKPOINT_TRACK));
        break;
    }

    case ACT_AUTOSCROLL: {
        spos = Vadd(spos, (FVec2){F_HALF_SCREEN_WIDTH, Fx1});
        player->bounds = level_info->bounds;
        set_player_track(player, VAL(spawn, SCROLL_TRACK));
        break;
    }
    }

    GameActor* pawn = create_actor(ACT_PLAYER, spos);
    if (pawn == NULL)
        goto rp_spectate;

    GameActor* old_pawn = get_actor(player->actor);
    if (old_pawn != NULL) {
        VAL(pawn, PLAYER_FLASH) = 100;
        FLAG_ON(old_pawn, FLG_DESTROY);

        play_state_sound("respawn", PLAY_POS, A_ACTOR(pawn));
    }

    pawn->player = player->id;
    FLAG_ON(pawn, spawn->flags & FLG_X_FLIP);

    switch (spawn->type) {
    default:
        break;

    case ACT_PLAYER_SPAWN: {
        if (!ANY_FLAG(spawn, FLG_PLAYER_WARP_OUT))
            break;

        VAL(pawn, PLAYER_WARP_OUT_ANGLE) = VAL(spawn, PLAYER_WARP_OUT_ANGLE);
        FLAG_ON(pawn, FLG_PLAYER_WARP_OUT);

        play_state_sound("warp", PLAY_POS, A_ACTOR(pawn));
        break;
    }

    case ACT_AUTOSCROLL: {
        if (!touching_solid(Radd(pawn->box, pawn->pos), SOL_SOLID))
            break;

        VAL(pawn, PLAYER_FLASH) = 100;
        FLAG_ON(pawn, FLG_PLAYER_DESCEND);
        break;
    }
    }

    player->actor = pawn->id;
    player->xscroll = Fx0;
    player->pos = pawn->pos;

    /// !!! CLIENT-SIDE !!!
    InterpPlayer* iplayer = &interp_state->players[player->id];
    iplayer->from = iplayer->to = iplayer->current = player->xscroll;

    if (player->id == local_player || view_player == NULL_PLAYER)
        set_view_player(player);
    /// !!! CLIENT-SIDE !!!

    return pawn;

rp_spectate:
    /// !!! CLIENT-SIDE !!!
    if (player->id == view_player) {
        for (PlayerID i = 0; i < game_context.num_players; i++) {
            const GamePlayer* oplayer = get_player(i);
            if (oplayer != NULL && oplayer->lives >= 0 && get_actor(oplayer->actor) != NULL)
                set_view_player(oplayer);
        }
    }
    /// !!! CLIENT-SIDE !!!

    return NULL;
}

Fixed get_player_jump(const GamePlayer* player) {
    if (player == NULL)
        return Fx1;

    const GameCharacter* character = get_character(game_context.players[player->id].character);
    return (character == NULL) ? Fx1 : character->jump;
}

const FVec2 nearest_player_pos(const FVec2 pos) {
    FVec2 ppos = {Fx0};
    Fixed score = FxUpper;

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER)
            continue;

        const Fixed dist = Vdist(pos, pawn->pos);
        if (dist < score) {
            ppos = player->pos;
            score = dist;
        }
    }

    return ppos;
}

void set_player_track(GamePlayer* player, Uint8 track) {
    if (player != NULL)
        player->track = track;
    update_player_track(player);
}

void update_player_track(const GamePlayer* player) {
    if (player == NULL || in_blocking_sequence())
        return;

    const GameActor* actor = get_actor(player->actor);
    if (actor != NULL && actor->type == ACT_PLAYER && VAL(actor, PLAYER_STARMAN) > 0) {
        play_state_track(player->id, "smw/starman", PLAY_LOOPING, 0);
        return;
    }

    if (player->track < 0 || player->track > MAX_GAME_TRACKS) {
        stop_state_track(player->id);
    } else {
        play_state_track(player->id, level_info->strings[GSTR_TRACK_START + player->track], PLAY_LOOPING,
            level_info->track_offsets[player->track]);
    }
}

Bool all_players_dead() {
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        if (player->lives >= 0)
            return FALSE;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER)
            return FALSE;
    }

    return TRUE;
}

void win_player(GamePlayer* player) {
    if (player == NULL)
        return;

    GameActor* actor = NULL;
    FOR_EACH_ACTOR (actor) {
        switch (actor->type) {
        default:
            break;

        case ACT_PLAYER: {
            if (actor->player != player->id)
                FLAG_ON(actor, FLG_DESTROY);

            VAL(actor, PLAYER_STARMAN) = 0;
            break;
        }

        case ACT_PLAYER_DEAD: {
            if (actor->player != player->id)
                FLAG_ON(actor, FLG_DESTROY);

            break;
        }

        case ACT_SUPER_MUSHROOM:
        case ACT_FIRE_FLOWER:
        case ACT_STARMAN:
        case ACT_1UP_MUSHROOM:
        case ACT_POISON_MUSHROOM:
        case ACT_GREEN_LUI:
        case ACT_BEETROOT: {
            FLAG_ON(actor, FLG_DESTROY);
            break;
        }

        case ACT_FIREBALL_PROJECTILE: {
            if (get_player(actor->player) != NULL) {
                give_points(actor, player, 100);
                FLAG_ON(actor, FLG_DESTROY);
            }

            break;
        }

        case ACT_BEETROOT_PROJECTILE: {
            if (get_player(actor->player) != NULL && !ANY_FLAG(actor, FLG_PROJECTILE_SINK)) {
                give_points(actor, player, 200);
                FLAG_ON(actor, FLG_DESTROY);
            }

            break;
        }

        case ACT_GOAL_BAR: {
            if (!ANY_FLAG(actor, FLG_BAR_FLY))
                actor->vel.y = Fx0;

            break;
        }
        }
    }

    set_sequence(GS_WIN, player, 0);

    set_view_player(player);
    play_state_track(player->id, (game_state->flags & GF_LOST_MAP) ? "smw/bonus_clear" : "smw/castle_clear", 0, 0);
}

// ======
// ACTORS
// ======

SolidFlags always_solid(const GameActor* actor) {
    (void)actor;

    return SOL_SOLID;
}

SolidFlags always_top(const GameActor* actor) {
    (void)actor;

    return SOL_TOP;
}

SolidFlags always_bottom(const GameActor* actor) {
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
            goto ca_found;

        index = (ActorID)((index + 1) % MAX_ACTORS);
    }

    WARN("Too many actors!!!");
    return NULL;

ca_found:
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

    actor->platform = NULL_ACTOR;
    FLAG_ON(actor, FLG_VISIBLE);
    TOUCH_ON(actor, TOUCH_BOTTOM);

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

ActorID get_num_actors(ActorType type) {
    ActorID num_actors = 0;

    const GameActor* actor = NULL;
    FOR_EACH_ACTOR (actor) {
        if (actor->type == type)
            ++num_actors;
    }

    return num_actors;
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

void push_actors(GameActor* actor) {
    if (actor == NULL || !ACTOR_IS_SOLID(actor, SOL_SOLID) || actor->sprout > 0
        || (actor->pos.x == actor->last_pos.x && actor->pos.y == actor->last_pos.y))
    {
        return;
    }

    const FRect abox = Radd(actor->box, actor->pos);
    Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
    Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
    cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
    cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
    cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
    cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

    for (Sint32 cx = cx1; cx <= cx2; cx++) {
        for (Sint32 cy = cy1; cy <= cy2; cy++) {
            GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
            while (next != NULL) {
                GameActor* other = next;
                next = get_actor(next->previous_cell);

                if (actor == other || !TOUCHING(other, TOUCH_DISPLACEABLE) || ANY_FLAG(other, FLG_DESTROY))
                    continue;

                FRect obox = Radd(other->box, other->pos);
                if (!Rcollide(abox, obox))
                    continue;

                FVec2 push = other->pos;
                if (obox.start.x < abox.start.x && obox.end.x > abox.start.x)
                    push.x = actor->pos.x + actor->box.start.x - other->box.end.x;
                else if (obox.end.x > abox.end.x && obox.start.x < abox.end.x)
                    push.x = actor->pos.x + actor->box.end.x - other->box.start.x;

                obox = Radd(other->box, push);
                if (Rcollide(abox, obox)) {
                    if (obox.start.y < abox.start.y && obox.end.y > abox.start.y)
                        push.y = actor->pos.y + actor->box.start.y - other->box.end.y;
                    else if (obox.end.y > abox.end.y && obox.start.y < abox.end.y)
                        push.y = actor->pos.y + actor->box.end.y - other->box.start.y;
                }

                if (!touching_solid(Radd(other->box, push), SOL_SOLID))
                    move_actor(other, push);
            }
        }
    }
}

static FRect get_autoscroll_cbox(const GameActor* autoscroll, Fixed edge) {
    const FVec2 cpos = {
        Fclamp(autoscroll->pos.x, level_info->bounds.start.x, level_info->bounds.end.x - F_SCREEN_WIDTH),
        Fclamp(autoscroll->pos.y, level_info->bounds.start.y, level_info->bounds.end.y - F_SCREEN_HEIGHT),
    };
    return (FRect){
        {cpos.x + edge,                  cpos.y + edge                  },
        {cpos.x + F_SCREEN_WIDTH - edge, cpos.y + F_SCREEN_HEIGHT - edge},
    };
}

static FRect get_player_cbox(const GamePlayer* player, Fixed edge) {
    const FVec2 cpos = {
        Fclamp(player->pos.x + player->xscroll, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
            player->bounds.end.x - F_HALF_SCREEN_WIDTH),
        Fclamp(
            player->pos.y, player->bounds.start.y + F_HALF_SCREEN_HEIGHT, player->bounds.end.y - F_HALF_SCREEN_HEIGHT),
    };
    return (FRect){
        {cpos.x - F_HALF_SCREEN_WIDTH + edge, cpos.y - F_HALF_SCREEN_HEIGHT + edge},
        {cpos.x + F_HALF_SCREEN_WIDTH - edge, cpos.y + F_HALF_SCREEN_HEIGHT - edge},
    };
}

static Bool cbox_in_view(const FVec2 pos, const FRect cbox, Bool ignore_top) {
    return pos.x < cbox.end.x && pos.x > cbox.start.x && pos.y < cbox.end.y && (ignore_top || pos.y > cbox.start.y);
}

Bool in_any_view(const FVec2 pos, Fixed edge, Bool ignore_top) {
    const GameActor* autoscroll = get_actor(game_state->autoscroll);
    if (autoscroll != NULL)
        return cbox_in_view(pos, get_autoscroll_cbox(autoscroll, edge), ignore_top);

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player != NULL && cbox_in_view(pos, get_player_cbox(player, edge), ignore_top))
            return TRUE;
    }

    return FALSE;
}

Bool in_player_view(const GamePlayer* player, const FVec2 pos, Fixed edge, Bool ignore_top) {
    if (player == NULL)
        return FALSE;

    const GameActor* autoscroll = get_actor(game_state->autoscroll);
    return cbox_in_view(
        pos, (autoscroll == NULL) ? get_player_cbox(player, edge) : get_autoscroll_cbox(autoscroll, edge), ignore_top);
}

static Bool autoscroll_cx_in_view(const GameActor* autoscroll, Fixed x, Fixed edge) {
    const Fixed cx = Fclamp(autoscroll->pos.x, level_info->bounds.start.x, level_info->bounds.end.x - F_SCREEN_WIDTH);
    return x < (cx + F_SCREEN_WIDTH - edge) && x > (cx + edge);
}

static Bool player_cx_in_view(const GamePlayer* player, Fixed x, Fixed edge) {
    const Fixed cx = Fclamp(player->pos.x + player->xscroll, player->bounds.start.x + F_HALF_SCREEN_WIDTH,
        player->bounds.end.x - F_HALF_SCREEN_WIDTH);
    return x < (cx + F_HALF_SCREEN_WIDTH - edge) && x > (cx - F_HALF_SCREEN_WIDTH + edge);
}

Bool in_any_x_view(Fixed x, Fixed edge) {
    const GameActor* autoscroll = get_actor(game_state->autoscroll);
    if (autoscroll != NULL)
        return autoscroll_cx_in_view(autoscroll, x, edge);

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player != NULL && player_cx_in_view(player, x, edge))
            return TRUE;
    }

    return FALSE;
}

Bool in_player_x_view(const GamePlayer* player, Fixed x, Fixed edge) {
    if (player == NULL)
        return FALSE;

    const GameActor* autoscroll = get_actor(game_state->autoscroll);
    return (autoscroll == NULL) ? player_cx_in_view(player, x, edge) : autoscroll_cx_in_view(autoscroll, x, edge);
}

Bool below_nearest_bounds(const FVec2 pos, Fixed edge) {
    FRect bounds = {0};
    Fixed score = FxUpper;
    Bool found = FALSE;

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const Fixed dist = Vdist(pos, (FVec2){
                                          Fclamp(pos.x, player->bounds.start.x, player->bounds.end.x),
                                          Fclamp(pos.y, player->bounds.start.y, player->bounds.end.y),
                                      });
        if (dist < score) {
            bounds = player->bounds;
            score = dist;
            found = TRUE;
        }
    }

    return found && pos.y > (bounds.end.y + edge);
}

void collide_actor(GameActor* actor) {
    if (actor == NULL || actor->sprout > 0)
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
            GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
            while (next != NULL) {
                GameActor* other = next;
                next = get_actor(next->previous_cell);

                if (actor == other || ANY_FLAG(other, FLG_DESTROY)
                    || !Rcollide(abox, Radd(other->box, Vadd(other->pos, (FVec2){Fx0, Int2Fx(other->sprout)}))))
                {
                    continue;
                }

                ACTOR_CALL(other, collide, actor);

                if (ANY_FLAG(actor, FLG_DESTROY))
                    return;
            }
        }
    }
}

Bool touching_solid(const FRect rect, SolidFlags types) {
    for (Uint8 i = 0; i < level_info->num_collisions; i++) {
        const CollisionMap* cmap = &level_info->collisions[i];
        if (!Rcollide(rect, cmap->bounds))
            continue;

        const FRect orect = Rsub(rect, cmap->bounds.start);
        Sint32 cx1 = (orect.start.x - cmap->cell_size.x) / cmap->cell_size.x,
               cy1 = (orect.start.y - cmap->cell_size.y) / cmap->cell_size.y;
        Sint32 cx2 = (orect.end.x + cmap->cell_size.x) / cmap->cell_size.x,
               cy2 = (orect.end.y + cmap->cell_size.y) / cmap->cell_size.y;
        cx1 = SDL_clamp(cx1, 0, cmap->size[0] - 1);
        cy1 = SDL_clamp(cy1, 0, cmap->size[1] - 1);
        cx2 = SDL_clamp(cx2, 0, cmap->size[0] - 1);
        cy2 = SDL_clamp(cy2, 0, cmap->size[1] - 1);

        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                const SolidFlags solid = cmap->grid[cx + (cy * cmap->size[0])];
                if (!(solid & types))
                    continue;

                const FVec2 cpos = (FVec2){cx * cmap->cell_size.x, cy * cmap->cell_size.y};
                const FRect crect = (FRect){cpos, Vadd(cpos, cmap->cell_size)};
                if (Rcollide(orect, crect))
                    return TRUE;
            }
        }
    }

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
                const SolidFlags solid = ACTOR_GET_SOLID(actor);
                if ((solid & types) && Rcollide(rect, Radd(actor->box, actor->pos)))
                    return TRUE;
            }
        }
    }

    return FALSE;
}

// NOLINTBEGIN(misc-no-recursion)
void displace_actor(GameActor* actor, Fixed climb, Bool unstuck) {
    if (actor == NULL)
        return;

    if (actor->sprout > 0) {
        TOUCH_OFF(actor, TOUCH_SIDES);
        return;
    }

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
        actor->vel.x = actor->vel.y = Fx0;
        TOUCH_ON(actor, TOUCH_STUCK);

        return;
    }

    TOUCH_ON(actor, TOUCH_DISPLACEABLE);

    const GameActor* platform = get_actor(actor->platform);
    if (platform != NULL) {
        actor->platform = NULL_ACTOR;

        const FVec2 avel = actor->vel;
        const FVec2 pvel = Vsub(platform->pos, platform->last_pos);

        actor->vel = pvel;
        displace_actor(actor, Fx0, FALSE);
        actor->vel = avel;

        if (actor->platform == platform->id) {
            if (!Rcollide(Radd(Radd(actor->box, actor->pos), (FVec2){pvel.x, pvel.y + Fx1}),
                    Radd(platform->box, platform->pos)))
            {
                actor->platform = NULL_ACTOR;
            }
        }
    }

    FVec2 npos = actor->pos;
    TOUCH_OFF(actor, TOUCH_SIDES);

    // Horizontal collision
    if (actor->vel.x != Fx0) {
        npos.x += actor->vel.x;
        FRect abox = Radd(actor->box, npos);
        Bool climbed = FALSE, stop = FALSE;

        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        const Bool right = actor->vel.x > Fx0;
        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
                while (next != NULL) {
                    GameActor* displacer = next;
                    next = get_actor(next->previous_cell);

                    if (actor == displacer || ANY_FLAG(displacer, FLG_DESTROY))
                        continue;

                    const SolidFlags solid = ACTOR_GET_SOLID(displacer);
                    if (!(solid & (SOL_SOLID | SOL_LEFT | SOL_RIGHT))
                        || (right && (solid & SOL_SLOPE_LEFT) == SOL_SLOPE_LEFT)
                        || (!right && (solid & SOL_SLOPE_RIGHT) == SOL_SLOPE_RIGHT))
                    {
                        continue;
                    }

                    const FRect dbox = Radd(displacer->box, displacer->pos);
                    if (!Rcollide(abox, dbox))
                        continue;

                    if ((solid & (SOL_SOLID | SOL_TOP)) && actor->vel.y >= Fx0
                        && (npos.y + actor->box.end.y - climb) < dbox.start.y)
                    {
                        const Fixed step = dbox.start.y - actor->box.end.y;
                        const FRect sbox = Radd(actor->box, (FVec2){npos.x + (right ? 1 : -1), step});

                        if (!touching_solid(sbox, SOL_SOLID)) {
                            ACTOR_CALL(displacer, on_top, actor);

                            npos.y = step;
                            actor->vel.y = Fx0;
                            TOUCH_ON(actor, TOUCH_BOTTOM);

                            climbed = TRUE;
                            goto da_climbed;
                        }
                    }

                    if (right) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_LEFT) && (npos.x + actor->box.end.x - actor->vel.x) <= dbox.start.x))
                        {
                            ACTOR_CALL(displacer, on_left, actor);
                            npos.x = Fmin(npos.x, dbox.start.x - actor->box.end.x);
                            stop |= actor->vel.x >= Fx0;
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_RIGHT) && (npos.x + actor->box.start.x - actor->vel.x) >= dbox.end.x))
                    {
                        ACTOR_CALL(displacer, on_right, actor);
                        npos.x = Fmax(npos.x, dbox.end.x - actor->box.start.x);
                        stop |= actor->vel.x <= Fx0;
                    }

                    climbed = FALSE;
                }
            }
        }

        abox = Radd(actor->box, npos);
        for (Uint8 i = 0; i < level_info->num_collisions; i++) {
            const CollisionMap* cmap = &level_info->collisions[i];
            if (!Rcollide(abox, cmap->bounds))
                continue;

            const FRect orect = Rsub(abox, cmap->bounds.start);
            Sint32 cx1 = (orect.start.x - cmap->cell_size.x) / cmap->cell_size.x,
                   cy1 = (orect.start.y - cmap->cell_size.y) / cmap->cell_size.y;
            Sint32 cx2 = (orect.end.x + cmap->cell_size.x) / cmap->cell_size.x,
                   cy2 = (orect.end.y + cmap->cell_size.y) / cmap->cell_size.y;
            cx1 = SDL_clamp(cx1, 0, cmap->size[0] - 1);
            cy1 = SDL_clamp(cy1, 0, cmap->size[1] - 1);
            cx2 = SDL_clamp(cx2, 0, cmap->size[0] - 1);
            cy2 = SDL_clamp(cy2, 0, cmap->size[1] - 1);

            for (Sint32 cx = cx1; cx <= cx2; cx++) {
                for (Sint32 cy = cy1; cy <= cy2; cy++) {
                    const SolidFlags solid = cmap->grid[cx + (cy * cmap->size[0])];
                    if (!(solid & (SOL_SOLID | SOL_LEFT | SOL_RIGHT))
                        || (right && (solid & SOL_SLOPE_LEFT) == SOL_SLOPE_LEFT)
                        || (!right && (solid & SOL_SLOPE_RIGHT) == SOL_SLOPE_RIGHT))
                    {
                        continue;
                    }

                    const FVec2 cpos
                        = Vadd(cmap->bounds.start, (FVec2){cx * cmap->cell_size.x, cy * cmap->cell_size.y});
                    const FRect cbox = (FRect){cpos, Vadd(cpos, cmap->cell_size)};
                    if (!Rcollide(abox, cbox))
                        continue;

                    if ((solid & (SOL_SOLID | SOL_TOP)) && actor->vel.y >= Fx0
                        && (npos.y + actor->box.end.y - climb) < cbox.start.y)
                    {
                        const Fixed step = cbox.start.y - actor->box.end.y;
                        const FRect sbox = Radd(actor->box, (FVec2){npos.x + (right ? 1 : -1), step});

                        if (!touching_solid(sbox, SOL_SOLID)) {
                            npos.y = step;
                            actor->vel.y = Fx0;
                            TOUCH_ON(actor, TOUCH_BOTTOM);

                            climbed = TRUE;
                            goto da_climbed;
                        }
                    }

                    if (right) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_LEFT) && (npos.x + actor->box.end.x - actor->vel.x) <= cbox.start.x))
                        {
                            npos.x = Fmin(npos.x, cbox.start.x - actor->box.end.x);
                            stop |= actor->vel.x >= Fx0;
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_RIGHT) && (npos.x + actor->box.start.x - actor->vel.x) >= cbox.end.x))
                    {
                        npos.x = Fmax(npos.x, cbox.end.x - actor->box.start.x);
                        stop |= actor->vel.x <= Fx0;
                    }

                    climbed = FALSE;
                }
            }
        }

    da_climbed:
        if (stop) {
            if (!climbed)
                TOUCH_ON(actor, right ? TOUCH_RIGHT : TOUCH_LEFT);
            actor->vel.x = Fx0;
        }
    }

    // Vertical collision
    if (actor->vel.y != Fx0) {
        npos.y += actor->vel.y;
        FRect abox = Radd(actor->box, npos);
        Bool stop = FALSE;

        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
                while (next != NULL) {
                    GameActor* displacer = next;
                    next = get_actor(next->previous_cell);

                    if (actor == displacer || ANY_FLAG(displacer, FLG_DESTROY))
                        continue;

                    const SolidFlags solid = ACTOR_GET_SOLID(displacer);
                    if (!(solid & (SOL_SOLID | SOL_TOP | SOL_BOTTOM)))
                        continue;

                    const FRect dbox = Radd(displacer->box, displacer->pos);
                    if (!Rcollide(abox, dbox))
                        continue;

                    if (solid & SOL_SLOPE) {
                        if (actor->vel.y < Fx0) {
                            if (!(solid & SOL_BOTTOM) || npos.y < dbox.end.y)
                                continue;

                            ACTOR_CALL(displacer, on_bottom, actor);
                            npos.y = Fmax(npos.y, dbox.end.y - actor->box.start.y);
                            stop |= actor->vel.y <= Fx0;
                        } else {
                            const Fixed width = Fabs(dbox.end.x - dbox.start.x);
                            if (width == Fx0)
                                continue;

                            const Bool side = (solid & SOL_LEFT) == SOL_LEFT;
                            const Fixed sa = side ? dbox.start.y : dbox.end.y, sb = side ? dbox.end.y : dbox.start.y,
                                        ax = npos.x + (side ? actor->box.start.x : actor->box.end.x);

                            const Fixed slope = Flerp(sa, sb, Fclamp(Fdiv(ax - dbox.start.x, width), Fx0, Fx1));
                            if ((npos.y + actor->box.end.y + Fabs(actor->vel.x)) >= slope) {
                                npos.y = slope - actor->box.end.y;
                                stop = TRUE;
                            }
                        }

                        continue;
                    }

                    if (actor->vel.y < Fx0) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_BOTTOM) && (npos.y + actor->box.start.y - actor->vel.y) >= dbox.end.y))
                        {
                            ACTOR_CALL(displacer, on_bottom, actor);
                            npos.y = Fmax(npos.y, dbox.end.y - actor->box.start.y);
                            stop |= actor->vel.y <= Fx0;
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_TOP)
                                   && (npos.y + actor->box.end.y - actor->vel.y) <= (dbox.start.y + climb)))
                    {
                        ACTOR_CALL(displacer, on_top, actor);
                        npos.y = Fmin(npos.y, dbox.start.y - actor->box.end.y);
                        stop |= actor->vel.y >= Fx0;
                    }
                }
            }
        }

        abox = Radd(actor->box, npos);
        for (Uint8 i = 0; i < level_info->num_collisions; i++) {
            const CollisionMap* cmap = &level_info->collisions[i];
            if (!Rcollide(abox, cmap->bounds))
                continue;

            const FRect orect = Rsub(abox, cmap->bounds.start);
            cx1 = (orect.start.x - cmap->cell_size.x) / cmap->cell_size.x;
            cy1 = (orect.start.y - cmap->cell_size.y) / cmap->cell_size.y;
            cx2 = (orect.end.x + cmap->cell_size.x) / cmap->cell_size.x;
            cy2 = (orect.end.y + cmap->cell_size.y) / cmap->cell_size.y;
            cx1 = SDL_clamp(cx1, 0, cmap->size[0] - 1);
            cy1 = SDL_clamp(cy1, 0, cmap->size[1] - 1);
            cx2 = SDL_clamp(cx2, 0, cmap->size[0] - 1);
            cy2 = SDL_clamp(cy2, 0, cmap->size[1] - 1);

            for (Sint32 cx = cx1; cx <= cx2; cx++) {
                for (Sint32 cy = cy1; cy <= cy2; cy++) {
                    const SolidFlags solid = cmap->grid[cx + (cy * cmap->size[0])];
                    if (!(solid & (SOL_SOLID | SOL_TOP | SOL_BOTTOM)))
                        continue;

                    const FVec2 cpos
                        = Vadd(cmap->bounds.start, (FVec2){cx * cmap->cell_size.x, cy * cmap->cell_size.y});
                    const FRect cbox = (FRect){cpos, Vadd(cpos, cmap->cell_size)};
                    if (!Rcollide(abox, cbox))
                        continue;

                    if (solid & SOL_SLOPE) {
                        if (actor->vel.y < Fx0) {
                            if (!(solid & SOL_BOTTOM) || npos.y < cbox.end.y)
                                continue;

                            npos.y = Fmax(npos.y, cbox.end.y - actor->box.start.y);
                            stop |= actor->vel.y <= Fx0;
                        } else {
                            const Fixed width = Fabs(cbox.end.x - cbox.start.x);
                            if (width == Fx0)
                                continue;

                            const Bool side = (solid & SOL_LEFT) == SOL_LEFT;
                            const Fixed sa = side ? cbox.start.y : cbox.end.y, sb = side ? cbox.end.y : cbox.start.y,
                                        ax = npos.x + (side ? actor->box.start.x : actor->box.end.x);

                            const Fixed slope = Flerp(sa, sb, Fclamp(Fdiv(ax - cbox.start.x, width), Fx0, Fx1));
                            if ((npos.y + actor->box.end.y + Fabs(actor->vel.x)) >= slope) {
                                npos.y = slope - actor->box.end.y;
                                stop = TRUE;
                            }
                        }

                        continue;
                    }

                    if (actor->vel.y < Fx0) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_BOTTOM) && (npos.y + actor->box.start.y - actor->vel.y) >= cbox.end.y))
                        {
                            npos.y = Fmax(npos.y, cbox.end.y - actor->box.start.y);
                            stop |= actor->vel.y <= Fx0;
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_TOP)
                                   && (npos.y + actor->box.end.y - actor->vel.y) <= (cbox.start.y + climb)))
                    {
                        npos.y = Fmin(npos.y, cbox.start.y - actor->box.end.y);
                        stop |= actor->vel.y >= Fx0;
                    }
                }
            }
        }

        if (stop) {
            TOUCH_ON(actor, (actor->vel.y < Fx0) ? TOUCH_TOP : TOUCH_BOTTOM);
            actor->vel.y = Fx0;
        }
    }

    move_actor(actor, npos);
}
// NOLINTEND(misc-no-recursion)

void displace_actor_soft(GameActor* actor) {
    if (actor == NULL)
        return;

    if (actor->sprout > 0) {
        TOUCH_OFF(actor, TOUCH_SIDES);
        return;
    }

    TOUCH_ON(actor, TOUCH_DISPLACEABLE);

    const FVec2 npos = Vadd(actor->pos, actor->vel);
    const FRect abox = Radd(actor->box, npos);
    TOUCH_OFF(actor, TOUCH_SIDES);

    // Horizontal collision
    if (actor->vel.x != Fx0) {
        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        const Bool right = actor->vel.x > Fx0;
        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
                while (next != NULL) {
                    GameActor* displacer = next;
                    next = get_actor(next->previous_cell);

                    if (actor == displacer || ANY_FLAG(displacer, FLG_DESTROY))
                        continue;

                    const SolidFlags solid = ACTOR_GET_SOLID(displacer);
                    if (!(solid & (SOL_SOLID | SOL_LEFT | SOL_RIGHT))
                        || (right && (solid & SOL_SLOPE_LEFT) == SOL_SLOPE_LEFT)
                        || (!right && (solid & SOL_SLOPE_RIGHT) == SOL_SLOPE_RIGHT))
                    {
                        continue;
                    }

                    const FRect dbox = Radd(displacer->box, displacer->pos);
                    if (!Rcollide(abox, dbox))
                        continue;

                    if (right) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_LEFT) && (npos.x + actor->box.end.x - actor->vel.x) <= dbox.start.x))
                        {
                            ACTOR_CALL(displacer, on_left, actor);
                            TOUCH_ON(actor, TOUCH_RIGHT);
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_RIGHT) && (npos.x + actor->box.start.x - actor->vel.x) >= dbox.end.x))
                    {
                        ACTOR_CALL(displacer, on_right, actor);
                        TOUCH_ON(actor, TOUCH_LEFT);
                    }
                }
            }
        }

        for (Uint8 i = 0; i < level_info->num_collisions; i++) {
            const CollisionMap* cmap = &level_info->collisions[i];
            if (!Rcollide(abox, cmap->bounds))
                continue;

            const FRect orect = Rsub(abox, cmap->bounds.start);
            Sint32 cx1 = (orect.start.x - cmap->cell_size.x) / cmap->cell_size.x,
                   cy1 = (orect.start.y - cmap->cell_size.y) / cmap->cell_size.y;
            Sint32 cx2 = (orect.end.x + cmap->cell_size.x) / cmap->cell_size.x,
                   cy2 = (orect.end.y + cmap->cell_size.y) / cmap->cell_size.y;
            cx1 = SDL_clamp(cx1, 0, cmap->size[0] - 1);
            cy1 = SDL_clamp(cy1, 0, cmap->size[1] - 1);
            cx2 = SDL_clamp(cx2, 0, cmap->size[0] - 1);
            cy2 = SDL_clamp(cy2, 0, cmap->size[1] - 1);

            for (Sint32 cx = cx1; cx <= cx2; cx++) {
                for (Sint32 cy = cy1; cy <= cy2; cy++) {
                    const SolidFlags solid = cmap->grid[cx + (cy * cmap->size[0])];
                    if (!(solid & (SOL_SOLID | SOL_LEFT | SOL_RIGHT))
                        || (right && (solid & SOL_SLOPE_LEFT) == SOL_SLOPE_LEFT)
                        || (!right && (solid & SOL_SLOPE_RIGHT) == SOL_SLOPE_RIGHT))
                    {
                        continue;
                    }

                    const FVec2 cpos
                        = Vadd(cmap->bounds.start, (FVec2){cx * cmap->cell_size.x, cy * cmap->cell_size.y});
                    const FRect cbox = (FRect){cpos, Vadd(cpos, cmap->cell_size)};
                    if (!Rcollide(abox, cbox))
                        continue;

                    if (right) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_LEFT) && (npos.x + actor->box.end.x - actor->vel.x) <= cbox.start.x))
                        {
                            TOUCH_ON(actor, TOUCH_RIGHT);
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_RIGHT) && (npos.x + actor->box.start.x - actor->vel.x) >= cbox.end.x))
                    {
                        TOUCH_ON(actor, TOUCH_LEFT);
                    }
                }
            }
        }
    }

    // Vertical collision
    if (actor->vel.y != Fx0) {
        Sint32 cx1 = (abox.start.x - CELL_SIZE) / CELL_SIZE, cy1 = (abox.start.y - CELL_SIZE) / CELL_SIZE;
        Sint32 cx2 = (abox.end.x + CELL_SIZE) / CELL_SIZE, cy2 = (abox.end.y + CELL_SIZE) / CELL_SIZE;
        cx1 = SDL_clamp(cx1, 0, MAX_CELLS - 1);
        cy1 = SDL_clamp(cy1, 0, MAX_CELLS - 1);
        cx2 = SDL_clamp(cx2, 0, MAX_CELLS - 1);
        cy2 = SDL_clamp(cy2, 0, MAX_CELLS - 1);

        for (Sint32 cx = cx1; cx <= cx2; cx++) {
            for (Sint32 cy = cy1; cy <= cy2; cy++) {
                GameActor* next = get_actor(game_state->grid[cx + (cy * MAX_CELLS)]);
                while (next != NULL) {
                    GameActor* displacer = next;
                    next = get_actor(next->previous_cell);

                    if (actor == displacer || ANY_FLAG(displacer, FLG_DESTROY))
                        continue;

                    const SolidFlags solid = ACTOR_GET_SOLID(displacer);
                    if (!(solid & (SOL_SOLID | SOL_TOP | SOL_BOTTOM)))
                        continue;

                    const FRect dbox = Radd(displacer->box, displacer->pos);
                    if (!Rcollide(abox, dbox))
                        continue;

                    if (solid & SOL_SLOPE) {
                        if (actor->vel.y < Fx0) {
                            if ((solid & SOL_BOTTOM) && npos.y >= dbox.end.y) {
                                ACTOR_CALL(displacer, on_bottom, actor);
                                TOUCH_ON(actor, TOUCH_TOP);
                            }
                        } else {
                            const Fixed width = dbox.end.x - dbox.start.x;
                            if (width == Fx0)
                                continue;

                            const Bool side = (solid & SOL_LEFT) == SOL_LEFT;
                            const Fixed sa = side ? dbox.start.y : dbox.end.y, sb = side ? dbox.end.y : dbox.start.y,
                                        ax = npos.x + (side ? actor->box.start.x : actor->box.end.x);

                            const Fixed slope = Flerp(sa, sb, Fclamp(Fdiv(ax - dbox.start.x, width), Fx0, Fx1));
                            if ((npos.y + actor->box.end.y + Fabs(actor->vel.x)) >= slope) {
                                ACTOR_CALL(displacer, on_top, actor);
                                TOUCH_ON(actor, TOUCH_BOTTOM);
                            }
                        }

                        continue;
                    }

                    if (actor->vel.y < Fx0) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_BOTTOM) && (npos.y + actor->box.start.y - actor->vel.y) >= dbox.end.y))
                        {
                            ACTOR_CALL(displacer, on_bottom, actor);
                            TOUCH_ON(actor, TOUCH_TOP);
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_TOP) && (npos.y + actor->box.end.y - actor->vel.y) <= dbox.start.y))
                    {
                        ACTOR_CALL(displacer, on_top, actor);
                        TOUCH_ON(actor, TOUCH_BOTTOM);
                    }
                }
            }
        }

        for (Uint8 i = 0; i < level_info->num_collisions; i++) {
            const CollisionMap* cmap = &level_info->collisions[i];
            if (!Rcollide(abox, cmap->bounds))
                continue;

            const FRect orect = Rsub(abox, cmap->bounds.start);
            cx1 = (orect.start.x - cmap->cell_size.x) / cmap->cell_size.x;
            cy1 = (orect.start.y - cmap->cell_size.y) / cmap->cell_size.y;
            cx2 = (orect.end.x + cmap->cell_size.x) / cmap->cell_size.x;
            cy2 = (orect.end.y + cmap->cell_size.y) / cmap->cell_size.y;
            cx1 = SDL_clamp(cx1, 0, cmap->size[0] - 1);
            cy1 = SDL_clamp(cy1, 0, cmap->size[1] - 1);
            cx2 = SDL_clamp(cx2, 0, cmap->size[0] - 1);
            cy2 = SDL_clamp(cy2, 0, cmap->size[1] - 1);

            for (Sint32 cx = cx1; cx <= cx2; cx++) {
                for (Sint32 cy = cy1; cy <= cy2; cy++) {
                    const SolidFlags solid = cmap->grid[cx + (cy * cmap->size[0])];
                    if (!(solid & (SOL_SOLID | SOL_TOP | SOL_BOTTOM)))
                        continue;

                    const FVec2 cpos
                        = Vadd(cmap->bounds.start, (FVec2){cx * cmap->cell_size.x, cy * cmap->cell_size.y});
                    const FRect cbox = (FRect){cpos, Vadd(cpos, cmap->cell_size)};
                    if (!Rcollide(abox, cbox))
                        continue;

                    if (solid & SOL_SLOPE) {
                        if (actor->vel.y < Fx0) {
                            if ((solid & SOL_BOTTOM) && npos.y >= cbox.end.y)
                                TOUCH_ON(actor, TOUCH_TOP);
                        } else {
                            const Fixed width = Fabs(cbox.end.x - cbox.start.x);
                            if (width == Fx0)
                                continue;

                            const Bool side = (solid & SOL_LEFT) == SOL_LEFT;
                            const Fixed sa = side ? cbox.start.y : cbox.end.y, sb = side ? cbox.end.y : cbox.start.y,
                                        ax = npos.x + (side ? actor->box.start.x : actor->box.end.x);

                            const Fixed slope = Flerp(sa, sb, Fclamp(Fdiv(ax - cbox.start.x, width), Fx0, Fx1));
                            if ((npos.y + actor->box.end.y + Fabs(actor->vel.x)) >= slope)
                                TOUCH_ON(actor, TOUCH_BOTTOM);
                        }

                        continue;
                    }

                    if (actor->vel.y < Fx0) {
                        if ((solid & SOL_SOLID)
                            || ((solid & SOL_BOTTOM) && (npos.y + actor->box.start.y - actor->vel.y) >= cbox.end.y))
                        {
                            TOUCH_ON(actor, TOUCH_TOP);
                        }
                    } else if ((solid & SOL_SOLID)
                               || ((solid & SOL_TOP) && (npos.y + actor->box.end.y - actor->vel.y) <= cbox.start.y))
                    {
                        TOUCH_ON(actor, TOUCH_BOTTOM);
                    }
                }
            }
        }
    }

    move_actor(actor, npos);
}

void draw_actor(const GameActor* actor, const char* sprite, Bool antijitter) {
    if (actor == NULL || !ANY_FLAG(actor, FLG_VISIBLE))
        return;

    FVec2 ipos = get_interp(actor);
    Fixed depth = actor->depth;
    if (actor->sprout > 0) {
        ipos.y += Int2Fx(actor->sprout);
        depth = Fmax(depth, Int2Fx(21));
    }

    if (antijitter) {
        const FVec2 cpos = videostate()->camera.pos;
        const Sint32 ax = Fx2Int(ipos.x - Ffrac(cpos.x)), ay = Fx2Int(ipos.y - Ffrac(cpos.y));
        batch_pos(B_F3(ax, ay, Fx2Float(depth)));
    } else {
        const Sint32 ax = Fx2Int(ipos.x), ay = Fx2Int(ipos.y);
        batch_pos(B_F3(ax, ay, Fx2Float(depth)));
    }
    batch_flip(B_B2(ANY_FLAG(actor, FLG_X_FLIP), ANY_FLAG(actor, FLG_Y_FLIP)));
    batch_sprite(sprite);
}

void draw_dead_actor(const GameActor* actor) {
    const ActorType type = VAL(actor, DEAD_TYPE);
    if (ACTORS[type] != NULL && ACTORS[type]->draw_dead != NULL)
        ACTORS[type]->draw_dead(actor);
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
    return (BAD_ACTOR(actor)) ? (FVec2){Fx0} : interp_state->actors[actor->id].current;
}

void skip_interp(const GameActor* actor) {
    if (BAD_ACTOR(actor))
        return;

    InterpActor* iactor = &interp_state->actors[actor->id];
    iactor->type = actor->type;
    iactor->from = iactor->to = iactor->current = actor->pos;
}

void align_interp(const GameActor* actor, const GameActor* from) {
    if (BAD_ACTOR(actor) || BAD_ACTOR(from))
        return;

    InterpActor *iactor = &interp_state->actors[actor->id], *ifrom = &interp_state->actors[from->id];
    iactor->from = ifrom->from;
    iactor->to = ifrom->to;
    iactor->current = ifrom->current;
}

#undef BAD_ACTOR
