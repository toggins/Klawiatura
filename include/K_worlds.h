#pragma once

struct WorldContext;

#include "K_file.h" // IWYU pragma: export
#include "K_game.h"

#define SAVE_HEADER "ksav0"

typedef struct {
    Bool has_map;
    Uint8 num_levels;
    Sint32 lives;

    Uint32 hash;

    const char *name, *description;
    TinyHash* levels;
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

typedef struct {
    Bool valid;
    PlayerCharacter character;
    PlayerPowerup powerup;
    Sint32 lives;
    TinyHash world, level;
} WorldSaveInfo;

void worlds_init(), worlds_teardown();

void rediscover_worlds();
const World *get_world(const char*), *get_world_key(TinyHash);
const char *next_world_from(const char*), *last_world_from(const char*);
yyjson_doc* load_world_json(const char*, const char**);

void world_iterate_start();
const World* world_iterate_next();

Sint32 get_world_lives(const World*);

WorldContext empty_world_context(), init_world_context(const World*);
void jump_to_world(const WorldContext*, Bool), start_world(const WorldContext*);
const WorldContext* worldcontext();

WorldSaveInfo world_has_save(const World*);
void load_world_save(const World*), save_world_save();
