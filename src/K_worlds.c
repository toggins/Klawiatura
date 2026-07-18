#include "K_cmd.h"
#include "K_file.h"
#include "K_interface.h"
#include "K_log.h"
#include "K_string.h"
#include "K_worlds.h"

static TinyMap worlds = {0};
static TinyHash* world_array = NULL;

static WorldContext world_context = {0};

static void nuke_world(void* ptr) {
    World* world = ptr;
    SDL_free((void*)world->name);
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

    for (size_t i = 0; i < size; i++)
        world.hash += ((Uint8*)buffer)[i];

    world.has_map = yyjson_is_obj(yyjson_obj_get(root, "map"));

    yyjson_doc_free(json);

    const TinyHash key = StHashStr(name);
    if (TinyMapGet(&worlds, key) == NULL)
        world_array = TinyDPush(world_array, &key);
    TinyMapPut(&worlds, key, &world, sizeof(world))->cleanup = nuke_world;
}

void worlds_init() {
    world_array = MakeTinyD(TinyHash);

    iterate_data_files("worlds/*", TRUE, iterate_world_file, NULL);

    TinyMapIterator iter = TinyMapIter(&worlds);

    if (TinyDLength(world_array) > 0)
        SDL_strlcpy(CLIENT.world, get_world_key(world_array[0])->name, sizeof(CLIENT.world));
}

void worlds_teardown() {
    FreeTinyMap(&worlds);
    FreeTinyD(world_array);
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

WorldContext empty_world_context() {
    WorldContext ctx = {0};

    ctx.num_players = 1;

    for (size_t i = 0; i < SDL_arraysize(ctx.players); i++)
        ctx.players[i].lives = DEFAULT_LIVES;

    return ctx;
}

WorldContext init_world_context(TinyHash world) {
    WorldContext ctx = empty_world_context();
    ctx.world = world;

    if (is_connected()) {
        ctx.num_players = (PlayerID)get_game_player_count();
        for (PlayerID i = 0; i < ctx.num_players; i++) {
            const NetID pid = player_to_peer((PlayerID)i);
            WorldPlayerContext* pctx = &ctx.players[i];

            pctx->character = get_peer_number(pid, "character");
            pctx->powerup = get_peer_number(pid, "powerup");
            pctx->lives = (Sint8)(pctx->lives - get_powerup_cost(pctx->powerup));
        }
    } else {
        WorldPlayerContext* pctx = &ctx.players[0];
        pctx->character = CLIENT.character;
        pctx->powerup = CLIENT.powerup;
        pctx->lives = (Sint8)(pctx->lives - get_powerup_cost(pctx->powerup));
    }

    return ctx;
}

void jump_to_world(const WorldContext* wctx, Bool as_host) {
    if (wctx == NULL || (as_host && !is_host()) || (!as_host && !is_leader()))
        return;

    const World* world = get_world_key(wctx->world);
    ASSUME(world, "Invalid world key %" SDL_PRIu64, wctx->world);

    spread_world_packet(wctx);
    if (world->has_map) {
        set_screen(SCR_MAP, wctx, sizeof(*wctx));
        return;
    }

    const char* error = NULL;
    yyjson_doc* json = load_world_json(world->name, &error);
    ASSUME(json, "Failed to load world \"%s\": %s", world->name, error);

    GameContext gctx = init_game_context(wctx,
        StHashStr(yyjson_get_str(yyjson_arr_get(yyjson_obj_get(yyjson_doc_get_root(json), "levels"), wctx->level))));
    yyjson_doc_free(json);
    jump_to_game(&gctx, as_host);
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
