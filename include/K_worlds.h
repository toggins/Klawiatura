#pragma once

struct WorldContext;

#include "K_file.h" // IWYU pragma: export
#include "K_game.h"

typedef struct {
    const char* name;
    Uint32 hash;
    Bool has_map;
} World;

typedef struct {
    Sint8 lives;
    Uint8 coins;
    PlayerCharacter character;
    PlayerPowerup powerup;
    Uint32 score;
} WorldPlayerContext;

typedef struct WorldContext {
    TinyHash world;
    Uint8 level;
    GameFlags flags;

    PlayerID winner, num_players;
    WorldPlayerContext players[MAX_PLAYERS];
} WorldContext;

void worlds_init(), worlds_teardown();

const World *get_world(const char*), *get_world_key(TinyHash);
const char *next_world_from(const char*), *last_world_from(const char*);
yyjson_doc* load_world_json(const char*, const char**);

WorldContext init_world_context(TinyHash);
void jump_to_world(const WorldContext*), start_world(const WorldContext*);
const WorldContext* worldcontext();
