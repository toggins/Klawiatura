#include "K_audio.h"
#include "K_file.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"
#include "K_worlds.h"

typedef struct {
    Bool cross;
    Sint32 pos[2];
} MapPoint;

typedef struct {
    Sint32 pos[2];
} MapPathNode;

typedef struct {
    Uint16 size[2];
    Sint32 camera_pos[2];

    float title_y, title_y_speed;

    float player_pos[2], player_speed, player_frame;
    size_t current_node, current_point;

    const char *title, *track;
    Surface* surface;
    TileMap* tilemap;
    MapPoint* points;
    MapPathNode* path;
} MapState;

static MapState* map_state = NULL;

static void start(const void* secret, size_t secret_size) {
    EXPECT(secret_size == sizeof(WorldContext), "Secret isn't WorldContext?");
    WORLD_CONTEXT = *(WorldContext*)secret;
    EXPECT(WORLD_CONTEXT.num_players >= 1 && WORLD_CONTEXT.num_players <= MAX_PLAYERS,
        "Invalid player count in world context! Expected 1..%i, got %i", MAX_PLAYERS, WORLD_CONTEXT.num_players);

    const World* world = get_world_key(WORLD_CONTEXT.world);
    EXPECT(world, "Invalid world key %" SDL_PRIu64, WORLD_CONTEXT.world);

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

    map_state = SDL_calloc(1, sizeof(*map_state));
    EXPECT(map_state, "Failed to allocate map state");

    map_state->title = SDL_strdup(fmt("ui/map/world/%s", world->name));
    EXPECT(map_state->title, "Failed to allocate map title");

    yyjson_val* jarray = yyjson_obj_get(jmap, "size");
    map_state->size[0] = yyjson_get_uint(yyjson_arr_get(jarray, 0));
    map_state->size[1] = yyjson_get_uint(yyjson_arr_get(jarray, 1));

    jarray = yyjson_obj_get(jmap, "backdrops");
    for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
        yyjson_val* jbackdrop = yyjson_arr_get(jarray, i);
        if (!yyjson_is_obj(jbackdrop))
            continue;

        if (map_state->tilemap == NULL)
            map_state->tilemap = create_tilemap();

        const char* sprite = yyjson_get_str(yyjson_obj_get(jbackdrop, "sprite"));
        load_sprite(sprite, FALSE);

        yyjson_val* jarr2 = yyjson_obj_get(jbackdrop, "pos");
        const float pos[3] = {
            (float)yyjson_get_num(yyjson_arr_get(jarr2, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jarr2, 1)),
            (float)yyjson_get_num(yyjson_arr_get(jarr2, 2)),
        };

        jarr2 = yyjson_obj_get(jbackdrop, "size");
        const Bool has_size = yyjson_is_arr(jarr2);
        const float size[2] = {
            (float)yyjson_get_num(yyjson_arr_get(jarr2, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jarr2, 1)),
        };

        jarr2 = yyjson_obj_get(jbackdrop, "flip");
        const Bool flip[2] = {
            yyjson_get_bool(yyjson_arr_get(jarr2, 0)),
            yyjson_get_bool(yyjson_arr_get(jarr2, 1)),
        };

        jarr2 = yyjson_obj_get(jbackdrop, "tile");
        const Bool tile[2] = {
            yyjson_get_bool(yyjson_arr_get(jarr2, 0)),
            yyjson_get_bool(yyjson_arr_get(jarr2, 1)),
        };

        jarr2 = yyjson_obj_get(jbackdrop, "colors");
        Uint8 colors[4][4] = {
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255}
        };
        for (size_t j = 0, n = yyjson_arr_size(jarr2); j < n && j < 4; j++) {
            yyjson_val* const jcolor = yyjson_arr_get(jarr2, j);
            for (size_t k = 0; k < 4; k++)
                colors[j][k] = yyjson_get_uint(yyjson_arr_get(jcolor, k));
        }

        add_tilemap(map_state->tilemap, sprite, pos, has_size ? size : NULL, flip, tile, colors);
    }

    jarray = yyjson_arr_get(yyjson_obj_get(jmap, "paths"), WORLD_CONTEXT.level);
    for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
        yyjson_val* jpoint = yyjson_arr_get(jarray, i);
        if (!yyjson_is_arr(jpoint))
            continue;

        if (map_state->path == NULL)
            map_state->path = MakeTinyDPro(sizeof(MapPathNode), sizeof(MapPathNode));

        MapPathNode node = {0};
        node.pos[0] = (Sint32)yyjson_get_sint(yyjson_arr_get(jpoint, 0));
        node.pos[1] = (Sint32)yyjson_get_sint(yyjson_arr_get(jpoint, 1));

        map_state->path = TinyDAppendPro(map_state->path, &node);
    }

    if (map_state->path != NULL) {
        jarray = yyjson_obj_get(jmap, "points");
        for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
            yyjson_val* jpoint = yyjson_arr_get(jarray, i);
            if (!yyjson_is_obj(jpoint))
                continue;

            if (map_state->points == NULL)
                map_state->points = MakeTinyDPro(sizeof(MapPoint), sizeof(MapPoint));

            MapPoint point = {0};
            yyjson_val* jarr2 = yyjson_obj_get(jpoint, "pos");
            point.pos[0] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarr2, 0));
            point.pos[1] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarr2, 1));
            point.cross = yyjson_get_bool(yyjson_obj_get(jpoint, "cross"));

            map_state->points = TinyDAppendPro(map_state->points, &point);
        }
    }

    const char* track = (map_state->path == NULL) ? "yi_score" : yyjson_get_str(yyjson_obj_get(jmap, "track"));
    if (track != NULL) {
        map_state->track = SDL_strdup(track);
        EXPECT(map_state->track, "Failed to allocate map track name");
    }

    yyjson_doc_free(json);

    // ===========
    // MAIN ASSETS
    // ===========

    load_sprite_num("ui/map/point/%u", 6, FALSE);
    load_sprite_num("ui/map/cross/%u", 11, FALSE);

    const GamePlayerContext* pctx = &WORLD_CONTEXT.players[0];
    load_sprite(get_character_sprite(pctx->character, pctx->powerup, PF_WALK1), FALSE);
    load_sprite(get_character_sprite(pctx->character, pctx->powerup, PF_WALK2), FALSE);
    load_sprite(get_character_sprite(pctx->character, pctx->powerup, PF_WALK3), FALSE);

    load_sprite(map_state->title, FALSE);
    if (map_state->path == NULL)
        load_sprite_num("ui/map/world/completed/%u", 16, FALSE);

    load_sprite("ui/map/logo", FALSE);
    load_sprite("ui/bezel_l", FALSE);
    load_sprite("ui/bezel_r", FALSE);
    load_track(map_state->track, FALSE);

    // ==================
    // START OF MAP FRAME
    // ==================

    map_state->surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);

    if (map_state->path != NULL) {
        map_state->title_y = -69.f;

        const MapPathNode* node = &map_state->path[0];
        map_state->player_pos[0] = (float)node->pos[0];
        map_state->player_pos[1] = (float)node->pos[1];
        map_state->player_speed = 1.f;

        const size_t n = TinyDLength(map_state->points);
        while (map_state->current_point < n) {
            if (map_state->player_pos[0] < (float)map_state->points[map_state->current_point].pos[0])
                break;

            ++map_state->current_point;
        }

        map_state->camera_pos[0]
            = SDL_clamp(map_state->player_pos[0], HALF_SCREEN_WIDTH, map_state->size[0] - HALF_SCREEN_WIDTH);
        map_state->camera_pos[1]
            = SDL_clamp(map_state->player_pos[1], HALF_SCREEN_HEIGHT, map_state->size[1] - HALF_SCREEN_HEIGHT);
    } else {
        map_state->title_y = -102.f;

        map_state->camera_pos[0] = map_state->size[0] - HALF_SCREEN_WIDTH;
        map_state->camera_pos[1] = HALF_SCREEN_HEIGHT;
    }

    play_generic_track(GTS_MAIN, map_state->track, PLAY_LOOPING);
}

static void end() {
    SDL_free((void*)map_state->title);
    SDL_free((void*)map_state->track);
    destroy_surface(map_state->surface);
    destroy_tilemap(map_state->tilemap);
    FreeTinyD(map_state->points);
    FreeTinyD(map_state->path);
    SDL_free(map_state);
}

static void tick() {
    map_state->title_y += map_state->title_y_speed;
    map_state->title_y_speed += 0.4f;
    if (map_state->title_y > 16.f) {
        map_state->title_y -= map_state->title_y_speed;
        map_state->title_y_speed = (map_state->title_y_speed * -0.5f) - 1.f;
    }

    Bool can_move = FALSE;
    if (map_state->path != NULL) {
        if (map_state->current_node < TinyDLength(map_state->path)) {
            if ((map_state->player_speed <= 1.f
                    && (kb_pressed(KB_JUMP) || kb_pressed(KB_FIRE) || kb_pressed(KB_UI_ENTER)))
                || (map_state->player_speed > 1.f && map_state->player_speed < 12.5f))
            {
                map_state->player_speed += map_state->player_speed;
                if (map_state->player_speed > 12.5f)
                    map_state->player_speed = 12.5f;
            }

            const MapPathNode* node = &map_state->path[map_state->current_node];
            const float nx = (float)node->pos[0], ny = (float)node->pos[1];

            const float dir = SDL_atan2f(ny - map_state->player_pos[1], nx - map_state->player_pos[0]);
            map_state->player_pos[0] += SDL_cosf(dir) * map_state->player_speed;
            map_state->player_pos[1] += SDL_sinf(dir) * map_state->player_speed;

            if (SDL_sqrtf(SDL_powf(nx - map_state->player_pos[0], 2.f) + SDL_powf(ny - map_state->player_pos[1], 2.f))
                < map_state->player_speed)
            {
                map_state->player_pos[0] = nx;
                map_state->player_pos[1] = ny;
                ++map_state->current_node;
            }

            map_state->player_frame += 0.48f;

            const Sint32 xmax = map_state->size[0] - HALF_SCREEN_WIDTH, ymax = map_state->size[1] - HALF_SCREEN_HEIGHT;
            map_state->camera_pos[0] = SDL_clamp(map_state->player_pos[0], HALF_SCREEN_WIDTH, xmax);
            map_state->camera_pos[1] = SDL_clamp(map_state->player_pos[1], HALF_SCREEN_HEIGHT, ymax);
        } else {
            if (kb_pressed(KB_JUMP) && is_leader()) {
                WorldContext ctx = WORLD_CONTEXT;
                ++ctx.level;
                spread_world_packet(&ctx);
                set_screen(SCR_MAP, &ctx, sizeof(ctx));
            }

            map_state->player_frame += 0.12f;
            can_move = TRUE;
        }

        if (map_state->current_point < TinyDLength(map_state->points)
            && map_state->player_pos[0] >= (float)map_state->points[map_state->current_point].pos[0])
        {
            ++map_state->current_point;
        }
    } else if (kb_pressed(KB_JUMP) || kb_pressed(KB_PAUSE)) {
        set_screen(SCR_MENU, NULL, 0);
    }

    if (can_move) {
        const Sint32 xmove = map_state->camera_pos[0] + (((Sint32)kb_down(KB_RIGHT) - (Sint32)kb_down(KB_LEFT)) * 5),
                     ymove = map_state->camera_pos[1] + (((Sint32)kb_down(KB_DOWN) - (Sint32)kb_down(KB_UP)) * 5);
        const Sint32 xmax = map_state->size[0] - HALF_SCREEN_WIDTH, ymax = map_state->size[1] - HALF_SCREEN_HEIGHT;
        map_state->camera_pos[0] = SDL_clamp(xmove, HALF_SCREEN_WIDTH, xmax);
        map_state->camera_pos[1] = SDL_clamp(ymove, HALF_SCREEN_HEIGHT, ymax);
    }
}

static void draw_ui() {
    push_surface(map_state->surface);
    clear_depth(1.f);

    // World
    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    const float cx = (float)map_state->camera_pos[0], cy = (float)map_state->camera_pos[1];
    glm_ortho(cx - HALF_SCREEN_WIDTH, cx + HALF_SCREEN_WIDTH, cy - HALF_SCREEN_HEIGHT,
        cy - (SCREEN_HEIGHT + HALF_SCREEN_HEIGHT), -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    draw_tilemap(map_state->tilemap);

    if (map_state->path != NULL) {
        batch_reset();

        const Uint32 t = (Uint32)(totalticks() * 0.5f);
        for (size_t i = 0; i < map_state->current_point; i++) {
            const MapPoint* point = &map_state->points[i];
            batch_pos(B_XYZ(point->pos[0], point->pos[1], 10.f));
            batch_sprite(point->cross ? fmt("ui/map/cross/%u", t % 11) : fmt("ui/map/point/%u", t % 6));
        }

        batch_pos(B_XY((int)map_state->player_pos[0], (int)map_state->player_pos[1]));
        batch_scale(B_SIZE(0.5f));
        const GamePlayerContext* pctx = &WORLD_CONTEXT.players[0];
        batch_sprite(get_character_sprite(
            pctx->character, pctx->powerup, PF_WALK1 + (int)SDL_fmodf(map_state->player_frame, 3.f)));
        batch_scale(B_SIZE(1.f));
    }

    // HUD
    batch_test_depth(FALSE);
    batch_write_depth(FALSE);

    glm_ortho(0.f, SCREEN_WIDTH, 0.f, -SCREEN_HEIGHT, -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, (int)map_state->title_y));

    batch_sprite(map_state->title);
    if (map_state->path == NULL) {
        const Sprite* tspr = get_sprite(map_state->title);
        const float tb = (tspr == NULL) ? 0.f : (tspr->size[1] - tspr->offset[1]);
        batch_pos(B_XY(HALF_SCREEN_WIDTH, (int)map_state->title_y + tb));
        batch_sprite(fmt("ui/map/world/completed/%u", (Uint32)(totalticks() * 0.5f) % 16));
    }

    batch_pos(B_SCREEN);
    batch_sprite("ui/map/logo");

    if (map_state->path != NULL && map_state->current_node >= TinyDLength(map_state->path)
        && SDL_fmodf(totalticks(), 25.f) < 12.5f)
    {
        batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 24.f));
        batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
        if (is_leader()) {
            batch_colors(B_MF_GREEN);
            batch_string("header", 32.f, LFMT("opt_press", 's', kb_label(KB_JUMP)));
        } else {
            batch_colors(B_MF_WHITE);
            batch_string("header", 32.f, LFMT("opt_waiting_for_leader"));
        }
    }

    batch_write_depth(TRUE);
    batch_test_depth(TRUE);

    pop_surface();

    batch_reset();
    batch_surface(map_state->surface);
    batch_sprite("ui/bezel_l");
    batch_sprite("ui/bezel_r");
}

const ScreenTable TAB_MAP = {
    .start = start,
    .end = end,
    .tick = tick,
    .draw_ui = draw_ui,
};
