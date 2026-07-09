#pragma once

#include "K_game.h"
#include "K_memory.h" // IWYU pragma: keep

typedef struct {
    const char* name;
    Uint32 hash;
    Bool has_map;
} World;

typedef struct {
    TinyHash world;
    Uint8 level;

    PlayerID winner, num_players;
    GamePlayerContext players[MAX_PLAYERS];
} WorldContext;

extern WorldContext WORLD_CONTEXT;

void worlds_init(), worlds_teardown();

const World *get_world(const char*), *get_world_key(TinyHash);
const char *next_world_from(const char*), *last_world_from(const char*);

WorldContext init_world_context();
void jump_to_world(const WorldContext*);
