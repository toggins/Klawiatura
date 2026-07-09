#include "K_game.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_net.h" // IWYU pragma: export

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

static GekkoSession* game_session = NULL;

GameContext queue_game_context = {0};
static GameContext game_context = {0};
GameState* GAME_STATE = NULL;

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
    return (cid < 0 || cid >= CHR_SIZE) ? "%u" : CHARACTERS[cid].cursor;
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

const GameContext* gamecontext() {
    return &game_context;
}

/// THIS IS A CLIENT-SIDE INDEX. DO NOT USE IN GAME STATE!!!
PlayerID localplayer() {
    return local_player;
}
