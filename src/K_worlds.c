#include "K_cmd.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_net.h"
#include "K_string.h"
#include "K_worlds.h"

static TinyMap worlds = {0};
static TinyHash* world_array = NULL;

static TinyMapIterator iterator = {0};

static WorldContext world_context = {0};

static void nuke_world(void* ptr) {
    World* world = ptr;

    SDL_free((void*)world->name);
    SDL_free((void*)world->description);
    SDL_free((void*)world->levels);
}

static void iterate_world_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    (void)userdata;

    const char* error = NULL;
    yyjson_doc* json = read_json(buffer, size, &error);
    ASSUME(json, "Failed to read world \"%s\": %s", filename, error);

    const char* name = filename_no_ext(file_basename(filename));

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        WTF("Expected world \"%s\" JSON root as object, got %s", name, yyjson_get_type_desc(root));
        yyjson_doc_free(json);
        return;
    }

    World world = {0};

    world.name = SDL_strdup(name);
    EXPECT(world.name, "Failed to allocate world \"%s\" name", name);

    yyjson_val* jval = yyjson_obj_get(root, "description");
    if (yyjson_is_str(jval)) {
        world.description = SDL_strdup(yyjson_get_str(jval));
        EXPECT(world.description, "Failed to allocate world \"%s\" description", name);
    }

    for (size_t i = 0; i < size; i++)
        world.hash += ((Uint8*)buffer)[i];

    world.has_map = yyjson_is_obj(yyjson_obj_get(root, "map"));

    jval = yyjson_obj_get(root, "levels");
    const size_t num_levels = yyjson_arr_size(jval);
    world.num_levels = SDL_min(num_levels, SDL_MAX_UINT8);

    if (world.num_levels > 0) {
        world.levels = SDL_calloc(world.num_levels, sizeof(TinyHash));
        EXPECT(world.levels, "Failed to allocate world \"%s\" levels", name);
        for (Uint8 i = 0; i < world.num_levels; i++)
            world.levels[i] = StHashStr(yyjson_get_str(yyjson_arr_get(jval, i)));
    }

    world.lives = DEFAULT_LIVES;
    jval = yyjson_obj_get(root, "lives");
    if (yyjson_is_int(jval))
        world.lives = (Sint32)yyjson_get_sint(jval);

    yyjson_doc_free(json);

    const TinyHash key = StHashStr(name);
    if (TinyMapGet(&worlds, key) == NULL) {
        if (world_array == NULL)
            world_array = MakeTinyDPro(1, sizeof(*world_array));
        world_array = TinyDPush(world_array, &key);
    }
    TinyMapPut(&worlds, key, &world, sizeof(world))->cleanup = nuke_world;
}

void worlds_init() {
    rediscover_worlds();
}

void worlds_teardown() {
    FreeTinyMap(&worlds);
    FreeTinyD(world_array);
}

void rediscover_worlds() {
    FreeTinyMap(&worlds);
    FreeTinyD(world_array);
    world_array = NULL;

    iterate_data_files("worlds/*", TRUE, iterate_world_file, NULL);
    if (TinyDLength(world_array) > 0)
        SDL_strlcpy(CLIENT.world, get_world_key(world_array[0])->name, sizeof(CLIENT.world));
    else
        CLIENT.world[0] = '\0';
}

const World* get_world(const char* name) {
    return get_world_key(StHashStr(name));
}

const World* get_world_key(TinyHash key) {
    return (World*)TinyMapGet(&worlds, key);
}

const char* next_world_from(const char* name) {
    const TinyHash key = StHashStr(name);

    const World* world = NULL;
    const size_t n = TinyDLength(world_array);
    for (size_t i = 0; i < n; i++) {
        if (key == world_array[i]) {
            world = get_world_key(world_array[(i + 1) % n]);
            break;
        }
    }

    if (world == NULL && n > 0)
        world = get_world_key(world_array[0]);
    return (world == NULL) ? NULL : world->name;
}

const char* last_world_from(const char* name) {
    const TinyHash key = StHashStr(name);

    const World* world = NULL;
    const size_t n = TinyDLength(world_array);
    for (size_t i = 0; i < n; i++) {
        if (key == world_array[i]) {
            world = get_world_key(world_array[(i > 0) ? (i - 1) : (n - 1)]);
            break;
        }
    }

    if (world == NULL && n > 0)
        world = get_world_key(world_array[0]);
    return (world == NULL) ? NULL : world->name;
}

void world_iterate_start() {
    iterator = TinyMapIter(&worlds);
}

const World* world_iterate_next() {
    return TinyMapNext(&iterator) ? iterator.data : NULL;
}

yyjson_doc* load_world_json(const char* name, const char** err) {
    const World* world = get_world(name);
    if (world == NULL) {
        if (err != NULL)
            *err = "World not found";

        return NULL;
    }

    size_t size = 0;
    const void* buffer = load_data_file(fmt("worlds/%s.json", world->name), &size);
    if (buffer == NULL) {
        if (err != NULL)
            *err = "Failed to load world file";

        return NULL;
    }

    Uint32 hash = 0;
    for (size_t i = 0; i < size; i++)
        hash += ((Uint8*)buffer)[i];
    if (hash != world->hash) {
        if (err != NULL)
            *err = "World file was tampered with!";

        SDL_free((void*)buffer);
        return NULL;
    }

    yyjson_doc* json = read_json(buffer, size, err);
    SDL_free((void*)buffer);

    return json;
}

Sint32 get_world_lives(const World* world) {
    return (world == NULL) ? DEFAULT_LIVES : world->lives;
}

WorldContext empty_world_context() {
    WorldContext ctx = {0};

    ctx.num_players = 1;

    for (size_t i = 0; i < SDL_arraysize(ctx.players); i++)
        ctx.players[i].lives = DEFAULT_LIVES;

    return ctx;
}

WorldContext init_world_context(const World* world) {
    WorldContext ctx = empty_world_context();
    ctx.world = (world == NULL) ? 0 : StHashStr(world->name);

    const Sint32 lives = get_world_lives(world);
    if (is_connected()) {
        ctx.num_players = (PlayerID)get_game_player_count();
        for (PlayerID i = 0; i < ctx.num_players; i++) {
            const NetID pid = player_to_peer((PlayerID)i);
            WorldPlayerContext* pctx = &ctx.players[i];

            pctx->character = get_peer_number(pid, "character");
            pctx->powerup = get_peer_number(pid, "powerup");
            pctx->lives = (Sint32)(lives - get_powerup_cost(pctx->powerup));
        }
    } else {
        WorldPlayerContext* pctx = &ctx.players[0];
        pctx->character = CLIENT.character;
        pctx->powerup = CLIENT.powerup;
        pctx->lives = (Sint32)(lives - get_powerup_cost(pctx->powerup));
    }

    return ctx;
}

void jump_to_world(const WorldContext* wctx, Bool as_host) {
    if (wctx == NULL || (as_host && !is_host()) || (!as_host && !is_leader()))
        return;

    const World* world = get_world_key(wctx->world);
    if (world == NULL) {
        if (get_screen() != SCR_MENU) {
            bail_from_game();
            set_screen(SCR_MENU, NULL, 0);
        }

        WTF("Invalid world key %" SDL_PRIu64, wctx->world);
        return;
    }

    spread_world_packet(wctx);
    if (world->has_map || (!world->has_map && wctx->level >= world->num_levels && wctx->num_players > 1)) {
        set_screen(SCR_MAP, wctx, sizeof(*wctx));
        return;
    }

    start_world(wctx);

    const char* error = NULL;
    yyjson_doc* json = load_world_json(world->name, &error);
    ASSUME(json, "Failed to load world \"%s\": %s", world->name, error);

    GameContext gctx = init_game_context(wctx,
        StHashStr(yyjson_get_str(yyjson_arr_get(yyjson_obj_get(yyjson_doc_get_root(json), "levels"), wctx->level))));
    yyjson_doc_free(json);
    jump_to_game(&gctx, as_host);

    save_world_save();
}

void start_world(const WorldContext* ctx) {
    EXPECT(ctx, "Null world context?");
    world_context = *ctx;

    EXPECT(world_context.winner >= 0 && world_context.winner < MAX_PLAYERS,
        "Invalid winner in world context! Expected 0..%i, got %i", MAX_PLAYERS - 1, world_context.winner);
    EXPECT(world_context.num_players >= 1 && world_context.num_players <= MAX_PLAYERS,
        "Invalid player count in world context! Expected 1..%i, got %i", MAX_PLAYERS, world_context.num_players);
}

const WorldContext* worldcontext() {
    return &world_context;
}

WorldSaveInfo world_has_save(const World* world) {
    WorldSaveInfo save = {0};

    if (world == NULL)
        return save;

    SDL_IOStream* io = stream_user_file(fmt("saves/%s.sav", world->name));
    if (io == NULL)
        return save;

    char header[sizeof(SAVE_HEADER)] = "";
    SDL_ReadIO(io, header, sizeof(header));
    if (SDL_strcmp(header, SAVE_HEADER) != 0) {
        SDL_CloseIO(io);
        return save;
    }

    save.valid = TRUE;
    save.world = StHashStr(world->name);

    Uint8 lidx = 0;
    SDL_ReadU8(io, &lidx);
    save.level = world->levels[SDL_min(lidx, world->num_levels - 1)];

    GameFlags dummy_flags = 0;
    SDL_ReadU16LE(io, &dummy_flags);

    SDL_ReadU8(io, &save.character);
    SDL_ReadU8(io, &save.powerup);
    SDL_ReadS32LE(io, &save.lives);

    SDL_CloseIO(io);
    return save;
}

void load_world_save(const World* world) {
    if (world == NULL || is_client())
        return;

    if (is_connected())
        peers_to_players();

    WorldContext ctx = init_world_context(world);

    SDL_IOStream* io = stream_user_file(fmt("saves/%s.sav", world->name));
    if (io == NULL) {
        WTF("Failed to load world \"%s\" save, resetting", world->name);
        goto new_save;
    }

    char header[sizeof(SAVE_HEADER)] = "";
    SDL_ReadIO(io, header, sizeof(header));
    if (SDL_strcmp(header, SAVE_HEADER) != 0) {
        SDL_CloseIO(io);
        WTF("World \"%s\" save has bad header, resetting", world->name);

        goto new_save;
    }

    SDL_ReadU8(io, &ctx.level);
    SDL_ReadU16LE(io, &ctx.flags);

    PlayerID i = 0;
    while (SDL_TellIO(io) < SDL_GetIOSize(io)) {
        SDL_ReadU8(io, &ctx.players[i].character);
        SDL_ReadU8(io, &ctx.players[i].powerup);
        SDL_ReadS32LE(io, &ctx.players[i].lives);
        SDL_ReadU8(io, &ctx.players[i].coins);
        SDL_ReadU32LE(io, &ctx.players[i].score);
        ++i;
    }

    SDL_CloseIO(io);

new_save:
    jump_to_world(&ctx, TRUE);
}

void save_world_save() {
    const World* world = get_world_key(world_context.world);
    if (world == NULL || world_context.level <= 0 || world_context.level >= world->num_levels)
        return;

    SDL_IOStream* io = SDL_IOFromDynamicMem();
    ASSUME(io, "Failed to save game: %s", SDL_GetError());

    SDL_WriteIO(io, SAVE_HEADER, sizeof(SAVE_HEADER));

    SDL_WriteU8(io, world_context.level);
    SDL_WriteU16LE(io, world_context.flags);

    for (PlayerID i = 0; i < world_context.num_players; i++) {
        const WorldPlayerContext* player = &world_context.players[i];
        SDL_WriteU8(io, player->character);
        SDL_WriteU8(io, player->powerup);
        SDL_WriteS32LE(io, player->lives);
        SDL_WriteU8(io, player->coins);
        SDL_WriteU32LE(io, player->score);
    }

    if (save_user_folder("saves") && save_user_stream(fmt("saves/%s.sav", world->name), io, SDL_TellIO(io)))
        INFO("Game saved");
    else
        WTF("Failed to save game");

    SDL_CloseIO(io);
}
