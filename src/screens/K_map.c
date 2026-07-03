#include "K_audio.h"
#include "K_file.h"
#include "K_interface.h"
#include "K_string.h"
#include "K_video.h"
#include "K_worlds.h"

typedef struct {
    const char* title;
    float title_y, title_y_speed;

    const char* track;

    Surface* surface;
    TileMap* tilemap;
} WorldState;

static WorldContext world_context = {0};
static WorldState* world_state = NULL;

static void start(const void* secret, size_t secret_size) {
    EXPECT(secret_size == sizeof(WorldContext), "Secret isn't WorldContext?");
    world_context = *(WorldContext*)secret;
    EXPECT(world_context.num_players >= 1 && world_context.num_players <= MAX_PLAYERS,
        "Invalid player count in world context! Expected 1..%i, got %i", MAX_PLAYERS, world_context.num_players);

    const World* world = get_world_key(world_context.world);
    EXPECT(world, "Invalid world hash %" PRIu64, world_context.world);

    size_t size = 0;
    const void* buffer = load_data_file(fmt("worlds/%s.json", world->name), &size);
    EXPECT(buffer, "Failed to load world \"%s\"", world->name);

    Uint32 hash = 0;
    for (size_t i = 0; i < size; i++)
        hash += ((Uint8*)buffer)[i];
    EXPECT(hash == world->hash, "World \"%s\" file was tampered with!", world->name);

    const char* error = NULL;
    yyjson_doc* json = read_json(buffer, size, &error);
    EXPECT(json, "Failed to parse world \"%s\": %s", world->name, error);
    SDL_free((void*)buffer);

    yyjson_val* root = yyjson_doc_get_root(json);
    EXPECT(yyjson_is_obj(root), "Expected world \"%s\" JSON root as object, got %s", world->name,
        yyjson_get_type_desc(root));

    yyjson_val* jmap = yyjson_obj_get(root, "map");
    EXPECT(yyjson_is_obj(jmap), "Expected world \"%s\" map data as object, got %s", world->name,
        yyjson_get_type_desc(jmap));

    world_state = SDL_calloc(1, sizeof(*world_state));
    EXPECT(world_state, "Failed to allocate world state");

    world_state->title = SDL_strdup(fmt("ui/map/world/%s", world->name));
    EXPECT(world_state->title, "Failed to allocate world title");

    const char* track = yyjson_get_str(yyjson_obj_get(jmap, "track"));
    if (track != NULL) {
        world_state->track = SDL_strdup(track);
        EXPECT(world_state->track, "Failed to allocate world track name");
    }

    yyjson_val* jbackdrops = yyjson_obj_get(jmap, "backdrops");
    for (size_t i = 0, n = yyjson_arr_size(jbackdrops); i < n; i++) {
        yyjson_val* jbackdrop = yyjson_arr_get(jbackdrops, i);
        if (!yyjson_is_obj(jbackdrop))
            continue;

        if (world_state->tilemap == NULL)
            world_state->tilemap = create_tilemap();

        const char* sprite = yyjson_get_str(yyjson_obj_get(jbackdrop, "sprite"));
        load_sprite(sprite, FALSE);

        yyjson_val* jarray = yyjson_obj_get(jbackdrop, "pos");
        const float pos[3] = {
            (float)yyjson_get_num(yyjson_arr_get(jarray, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jarray, 1)),
            (float)yyjson_get_num(yyjson_arr_get(jarray, 2)),
        };

        jarray = yyjson_obj_get(jbackdrop, "size");
        const Bool has_size = yyjson_is_arr(jarray);
        const float size[2] = {
            (float)yyjson_get_num(yyjson_arr_get(jarray, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jarray, 1)),
        };

        jarray = yyjson_obj_get(jbackdrop, "flip");
        const Bool flip[2] = {
            yyjson_get_bool(yyjson_arr_get(jarray, 0)),
            yyjson_get_bool(yyjson_arr_get(jarray, 1)),
        };

        jarray = yyjson_obj_get(jbackdrop, "tile");
        const Bool tile[2] = {
            yyjson_get_bool(yyjson_arr_get(jarray, 0)),
            yyjson_get_bool(yyjson_arr_get(jarray, 1)),
        };

        jarray = yyjson_obj_get(jbackdrop, "colors");
        Uint8 colors[4][4] = {
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255}
        };

        for (size_t j = 0; j < yyjson_arr_size(jarray) && j < 4; j++) {
            yyjson_val* const jcolor = yyjson_arr_get(jarray, j);

            for (size_t k = 0; k < 4; k++)
                colors[j][k] = (Uint8)yyjson_get_uint(yyjson_arr_get(jcolor, k));
        }

        add_tilemap(world_state->tilemap, sprite, pos, has_size ? size : NULL, flip, tile, colors);
    }

    yyjson_doc_free(json);

    // ===========
    // MAIN ASSETS
    // ===========

    load_sprite(world_state->title, FALSE);
    load_sprite("ui/map/logo", FALSE);
    load_sprite("ui/bezel_l", FALSE);
    load_sprite("ui/bezel_r", FALSE);
    load_track(world_state->track, FALSE);

    // ==================
    // START OF MAP FRAME
    // ==================

    world_state->surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);
    world_state->title_y = -69.f;

    play_generic_track(GTS_MAIN, world_state->track, PLAY_LOOPING);
}

static void end() {
    SDL_free((void*)world_state->title);
    SDL_free((void*)world_state->track);
    destroy_surface(world_state->surface);
    destroy_tilemap(world_state->tilemap);
    SDL_free(world_state);
}

static void tick() {
    world_state->title_y += world_state->title_y_speed;
    world_state->title_y_speed += 0.4f;
    if (world_state->title_y > 16.f) {
        world_state->title_y -= world_state->title_y_speed;
        world_state->title_y_speed = (world_state->title_y_speed * -0.5f) - 1.f;
    }
}

static void draw_ui() {
    push_surface(world_state->surface);
    clear_depth(1.f);

    draw_tilemap(world_state->tilemap);

    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, (int)world_state->title_y));
    batch_sprite(world_state->title);
    batch_pos(B_SCREEN);
    batch_sprite("ui/map/logo");

    pop_surface();

    batch_reset();
    batch_surface(world_state->surface);
    batch_sprite("ui/bezel_l");
    batch_sprite("ui/bezel_r");
}

const ScreenTable TAB_MAP = {
    .start = start,
    .end = end,
    .tick = tick,
    .draw_ui = draw_ui,
};
