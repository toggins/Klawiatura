#include "K_cmd.h"
#include "K_file.h"
#include "K_log.h"
#include "K_worlds.h"

static TinyMap worlds = {0};
static TinyHash* world_array = NULL;

WorldContext WORLD_CONTEXT = {0};

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
    INFO("World \"%s\" hash is %u", name, world.hash);

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

WorldContext init_world_context() {
    WorldContext ctx = {0};

    ctx.num_players = 1;
    for (size_t i = 0; i < SDL_arraysize(ctx.players); i++)
        ctx.players[i].lives = DEFAULT_LIVES;

    return ctx;
}
