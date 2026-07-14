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
#include "K_video.h"

typedef struct {
    GameState game;
    AudioState audio;
} SaveState;

// `extern` in K_actors.c
const GameActorTable* ACTORS[ACT_SIZE] = {0};

static const GameCharacter CHARACTERS[CHR_SIZE] = {
    [CHR_MARIO] = {
        .name = "Mario",
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
};

static Uint32 game_hash = 0;

static PlayerID local_player = NULL_PLAYER, view_player = NULL_PLAYER;

static Surface* game_surface = NULL;
static GekkoSession* game_session = NULL;

GameContext queue_game_context = {0};
static GameContext game_context = {0};

static LevelInfo* level_info = NULL;
static GameState* game_state = NULL;

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

void jump_to_game(const GameContext* ctx) {
    if (ctx == NULL || !is_leader())
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
    for (PlayerID i = 0L; i < MAX_PLAYERS; i++) {
        game_state->players[i].id = NULL_PLAYER;
        game_state->players[i].actor = NULL_ACTOR;
        for (ActorID j = 0L; j < MAX_PROJECTILES; j++)
            game_state->players[i].projectiles[j] = NULL_ACTOR;
        for (ActorID j = 0L; j < MAX_SINKING_PROJECTILES; j++)
            game_state->players[i].sinking_projectiles[j] = NULL_ACTOR;
    }

    game_state->live_actors = NULL_ACTOR;
    for (ActorID i = 0L; i < MAX_ACTORS; i++) {
        game_state->actors[i].id = game_state->actors[i].previous = game_state->actors[i].next
            = game_state->actors[i].previous_cell = game_state->actors[i].next_cell = NULL_ACTOR;
    }
    for (Sint32 i = 0L; i < GRID_SIZE; i++)
        game_state->grid[i] = NULL_ACTOR;

    game_state->spawn = game_state->checkpoint = NULL_ACTOR;
    game_state->water = 2147483647;

    game_state->clock = -1L;

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
    const size_t narr = yyjson_arr_size(jval);
    for (GameWarpID i = 0, n = SDL_min(narr, MAX_GAME_WARPS); i < n; i++)
        level_info->warps[i] = StHashStr(yyjson_get_str(yyjson_arr_get(jval, i)));

    jval = yyjson_obj_get(root, "time");
    if (yyjson_is_int(jval))
        game_state->clock = (Sint32)yyjson_get_sint(jval);

    jval = yyjson_obj_get(root, "backdrops");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n; i++) {
        yyjson_val* jbackdrop = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jbackdrop))
            continue;

        const char* sprite = yyjson_get_str(yyjson_obj_get(jbackdrop, "sprite"));
        load_sprite(sprite, AKL_NEVER);

        jval2 = yyjson_obj_get(jbackdrop, "pos");
        const float pos[3] = {
            (float)yyjson_get_num(yyjson_arr_get(jval2, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jval2, 1)),
            (float)yyjson_get_num(yyjson_arr_get(jval2, 2)),
        };

        jval2 = yyjson_obj_get(jbackdrop, "size");
        const Bool has_size = yyjson_is_arr(jval2);
        const float size[2] = {
            (float)yyjson_get_num(yyjson_arr_get(jval2, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jval2, 1)),
        };

        jval2 = yyjson_obj_get(jbackdrop, "flip");
        const Bool flip[2] = {
            yyjson_get_bool(yyjson_arr_get(jval2, 0)),
            yyjson_get_bool(yyjson_arr_get(jval2, 1)),
        };

        jval2 = yyjson_obj_get(jbackdrop, "tile");
        const Bool tile[2] = {
            yyjson_get_bool(yyjson_arr_get(jval2, 0)),
            yyjson_get_bool(yyjson_arr_get(jval2, 1)),
        };

        jval2 = yyjson_obj_get(jbackdrop, "colors");
        Uint8 colors[4][4] = {
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255}
        };
        for (size_t j = 0, n = yyjson_arr_size(jval2); j < n && j < 4; j++) {
            yyjson_val* const jcolor = yyjson_arr_get(jval2, j);
            for (size_t k = 0; k < 4; k++)
                colors[j][k] = yyjson_get_uint(yyjson_arr_get(jcolor, k));
        }

        add_tilemap(videostate()->tilemap, sprite, pos, has_size ? size : NULL, flip, tile, colors);
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

    // Load assets
    load_sprite_num("ui/coins/%u", 4, AKL_NEVER);
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
    local_player = view_player = populate_game(game_session, game_context.num_players);

    // Surface
    game_surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);

    // Initial game state
    load_level(game_context.level);

    for (PlayerID i = 0; i < game_context.num_players; i++) {
        GamePlayer* player = &game_state->players[i];
        player->id = i;

        const GamePlayerContext* pctx = &game_context.players[i];
        player->powerup = pctx->powerup;
        player->lives = pctx->lives;
        player->coins = pctx->coins;
        player->score = pctx->score;
    }

    play_state_track(STS_MAIN, level_info->strings[GSTR_TRACK1], PLAY_LOOPING);
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

static void tick_game_state(GameInput inputs[MAX_PLAYERS]) {
    for (PlayerID i = 0; i < game_context.num_players; i++) {
        GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        // Apply input
        player->last_input = player->input;
        player->input = inputs[i];
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
}

void tick_game() {
    if (game_session == NULL || (game_state->flags & GF_END))
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
        // LOCAL PLAYER GLOSSARY:
        // 0 .. (MAX_PLAYERS - 1) = Solo/online.
        // MAX_PLAYERS or higher = Spectator.
        if (local_player < MAX_PLAYERS) {
            gekko_add_local_input(game_session, local_player,
                &(GameInput){(kb_down(KB_UP) * GI_UP) | (kb_down(KB_LEFT) * GI_LEFT) | (kb_down(KB_DOWN) * GI_DOWN)
                             | (kb_down(KB_RIGHT) * GI_RIGHT) | (kb_down(KB_JUMP) * GI_JUMP)
                             | (kb_down(KB_RUN) * GI_RUN) | (kb_down(KB_FIRE) * GI_FIRE)});
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

static void draw_game_state() {
    clear_color(0.f, 0.f, 0.f, 1.f);
    clear_depth(1.f);
    batch_filter(FALSE);

    draw_tilemap(videostate()->tilemap);

    const GamePlayer* player = get_player(view_player);
    if (player == NULL)
        return;

    batch_write_depth(FALSE);
    batch_test_depth(FALSE);

    batch_reset();
    batch_pos(B_XY(32.f, 16.f));
    const char* cname = get_character_name(game_context.players[player->id].character);
    batch_string("hud", 16.f, fmt("%s × %i", cname, player->lives));
    batch_pos(B_XY(32.f + string_width("hud", 16.f, fmt("%s × 0", cname)), 34.f));
    batch_align(B_TOP_RIGHT);
    batch_string("hud", 16.f, fmt("%u", player->score));

    batch_pos(B_XY(224.f, 34.f));
    batch_sprite(fmt("ui/coins/%i", (int)((float)(game_state->time) / 6.25f) % 4));
    batch_pos(B_XY(234.f, 34.f));
    batch_align(B_TOP_LEFT);
    batch_string("hud", 16.f, fmt(" × %u", player->coins));

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

    batch_test_depth(TRUE);
    batch_write_depth(TRUE);

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

/// THIS IS A CLIENT-SIDE INDEX. DO NOT USE IN GAME STATE!!!
PlayerID localplayer() {
    return local_player;
}

GamePlayer* get_player(PlayerID pid) {
    return (pid < 0 || pid >= MAX_PLAYERS || game_state->players[pid].id != pid) ? NULL : &game_state->players[pid];
}
