#include "K_game.h"
#include "K_log.h"
#include "K_net.h" // IWYU pragma: export

// `extern` in S_actors.c
const GameActorTable* ACTORS[ACT_SIZE] = {0};

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
