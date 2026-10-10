#pragma once

struct WorldContext;

#include "K_file.h" // IWYU pragma: export
#include "K_game.h"

typedef struct {
    Bool has_map;
    Uint8 num_levels;
    Sint32 lives;

    const char* name;
    Uint32 hash;
} World;

typedef struct {
    Uint8 coins;
    PlayerCharacter character;
    PlayerPowerup powerup;
    Uint32 score;
    Sint32 lives;
} WorldPlayerContext;

typedef struct WorldContext {
    TinyHash world;
    Uint8 level;
    GameFlags flags;

    PlayerID winner, num_players;
    WorldPlayerContext players[MAX_PLAYERS];
} WorldContext;

void worlds_init(), worlds_teardown();

void rediscover_worlds();
const World *get_world(const char*), *get_world_key(TinyHash);
const char *next_world_from(const char*), *last_world_from(const char*);
yyjson_doc* load_world_json(const char*, const char**);

Sint32 get_world_lives(const World*);

WorldContext empty_world_context(), init_world_context(TinyHash);
void jump_to_world(const WorldContext*, Bool), start_world(const WorldContext*);
const WorldContext* worldcontext();
