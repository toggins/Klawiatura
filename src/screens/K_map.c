#include "K_audio.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"
#include "K_worlds.h"

typedef struct {
    float y, y_speed;
    struct {
        float from, to, y;
    } interp;
} MapLabel;

typedef struct {
    Uint8 frame;
    float pos[2], speed[2];
    struct {
        float from[2], to[2], pos[2];
    } interp;
} MapBubble;

typedef struct {
    Uint8 appear_speed;
    Bool swimming;
    float pos[2], speed, frame;
    struct {
        float from[2], to[2], pos[2];
    } interp;
    MapBubble* bubbles;
} MapPlayer;

typedef struct {
    float pos[2];
    struct {
        float from[2], to[2], pos[2];
    } interp;
} MapCamera;

typedef struct {
    float frame;
    struct {
        float from, to, frame;
    } interp;
} MapTransition;

typedef struct {
    Uint8 appear;
    Bool cross;
    Sint32 pos[2];
} MapPoint;

typedef struct {
    Sint32 pos[2];
} MapPathNode;

typedef struct {
    Uint8 ambush;
    Uint16 size[2];
    Sint32 water[4];

    MapLabel label;
    MapPlayer player;
    MapCamera camera;
    MapTransition transition;

    TinyHash level;
    size_t current_node;

    const char *title, *track;
    Surface* surface;
    TileMap* tilemap;
    MapPoint* points;
    MapPathNode* path;
} MapState;

static MapState* map_state = NULL;

static void start(const void* secret, size_t secret_size) {
    EXPECT(secret_size == sizeof(WorldContext), "Secret isn't WorldContext?");
    start_world(secret);
    const WorldContext* wctx = worldcontext();

    const World* world = get_world_key(wctx->world);
    EXPECT(world, "Invalid world key %" SDL_PRIu64, wctx->world);

    const char* error = NULL;
    yyjson_doc* json = load_world_json(world->name, &error);
    EXPECT(json, "Failed to load world \"%s\": %s", world->name, error);

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

    map_state->tilemap = create_tilemap();
    read_tilemap(map_state->tilemap, yyjson_obj_get(jmap, "backdrops"));

    yyjson_val* jpath = yyjson_arr_get(yyjson_obj_get(jmap, "paths"), wctx->level);
    Uint32 offset = 0;
    if (yyjson_is_obj(jpath)) {
        map_state->ambush = yyjson_get_bool(yyjson_obj_get(jpath, "ambush"));

        jarray = yyjson_obj_get(jpath, "nodes");
        for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
            yyjson_val* jnode = yyjson_arr_get(jarray, i);
            if (!yyjson_is_arr(jnode))
                continue;

            if (map_state->path == NULL)
                map_state->path = MakeTinyDPro(sizeof(MapPathNode), sizeof(MapPathNode));

            MapPathNode node = {0};
            node.pos[0] = (Sint32)yyjson_get_sint(yyjson_arr_get(jnode, 0));
            node.pos[1] = (Sint32)yyjson_get_sint(yyjson_arr_get(jnode, 1));

            map_state->path = TinyDPush(map_state->path, &node);
        }

        jarray = yyjson_obj_get(jpath, "water");
        map_state->water[0] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarray, 0));
        map_state->water[1] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarray, 1));
        map_state->water[2] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarray, 2));
        map_state->water[3] = (Sint32)yyjson_get_sint(yyjson_arr_get(jarray, 3));

        offset = yyjson_get_uint(yyjson_obj_get(jpath, "offset"));
    }

    if (map_state->path != NULL) {
        jarray = yyjson_obj_get(jmap, "points");
        for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
            yyjson_val* jpoint = yyjson_arr_get(jarray, i);
            if (!yyjson_is_arr(jpoint))
                continue;

            if (map_state->points == NULL)
                map_state->points = MakeTinyDPro(sizeof(MapPoint), sizeof(MapPoint));

            MapPoint point = {0};
            point.pos[0] = (Sint32)yyjson_get_sint(yyjson_arr_get(jpoint, 0));
            point.pos[1] = (Sint32)yyjson_get_sint(yyjson_arr_get(jpoint, 1));
            point.cross = yyjson_get_bool(yyjson_arr_get(jpoint, 2));

            map_state->points = TinyDPush(map_state->points, &point);
        }
    }

    map_state->level = StHashStr(yyjson_get_str(yyjson_arr_get(yyjson_obj_get(root, "levels"), wctx->level)));

    const char* track = (map_state->path == NULL) ? "yi/score" : yyjson_get_str(yyjson_obj_get(jmap, "track"));
    if (track != NULL) {
        map_state->track = SDL_strdup(track);
        EXPECT(map_state->track, "Failed to allocate map track name");
    }

    yyjson_doc_free(json);

    // ===========
    // MAIN ASSETS
    // ===========

    load_sprite_num("ui/map/point/%u", 6, AKL_NEVER);
    load_sprite_num("ui/map/cross/%u", 11, AKL_NEVER);

    load_sprite(map_state->title, AKL_NEVER);
    if (map_state->path == NULL) {
        load_sprite_num("ui/map/world/completed/%u", 16, AKL_NEVER);
    } else {
        const WorldPlayerContext* pctx = &wctx->players[wctx->winner];
        for (PlayerFrame i = PF_WALK1; i <= (PlayerFrame)PF_WALK3; i++)
            load_sprite(get_character_sprite(pctx->character, pctx->powerup, i), AKL_NEVER);
        for (PlayerFrame i = PF_SWIM1; i <= (PlayerFrame)PF_SWIM8; i++)
            load_sprite(get_character_sprite(pctx->character, pctx->powerup, i), AKL_NEVER);

        load_sprite("ui/map/bubble", AKL_NEVER);

        if (map_state->ambush > 0) {
            load_sprite("ui/map/bro", AKL_NEVER);
            load_sound("ambush", AKL_NEVER);
            load_sound(get_character_voice(pctx->character, PV_PANIC), AKL_NEVER);
        }
    }

    load_sprite("ui/map/logo", AKL_NEVER);
    load_sprite("ui/bezel_l", AKL_NEVER);
    load_sprite("ui/bezel_r", AKL_NEVER);
    load_sound("ui/enter", AKL_ONCE);
    load_track(map_state->track, AKL_NEVER);
    load_ui(UI_PAUSE);

    // ==================
    // START OF MAP FRAME
    // ==================

    map_state->surface = create_surface(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE, TRUE);

    if (map_state->path != NULL) {
        MapLabel* label = &map_state->label;
        label->y = label->interp.from = label->interp.to = label->interp.y = -69.f;

        const MapPathNode* node = &map_state->path[0];
        MapPlayer* player = &map_state->player;
        player->pos[0] = player->interp.from[0] = player->interp.to[0] = player->interp.pos[0] = (float)node->pos[0];
        player->pos[1] = player->interp.from[1] = player->interp.to[1] = player->interp.pos[1] = (float)node->pos[1];
        player->speed = 1.f;
        player->appear_speed = 1;

        for (size_t i = 0, n = TinyDLength(map_state->points); i < n; i++) {
            MapPoint* point = &map_state->points[i];
            if ((map_state->player.pos[0] + 8.f) < (float)(point->pos[0] + 7))
                break;

            point->appear = 21;
        }

        MapCamera* camera = &map_state->camera;
        camera->pos[0] = camera->interp.from[0] = camera->interp.to[0] = camera->interp.pos[0]
            = SDL_clamp(map_state->player.pos[0], HALF_SCREEN_WIDTH, map_state->size[0] - HALF_SCREEN_WIDTH);
        camera->pos[1] = camera->interp.from[1] = camera->interp.to[1] = camera->interp.pos[1]
            = SDL_clamp(map_state->player.pos[1], HALF_SCREEN_HEIGHT, map_state->size[1] - HALF_SCREEN_HEIGHT);
    } else {
        MapLabel* label = &map_state->label;
        label->y = label->interp.from = label->interp.to = label->interp.y = -102.f;

        MapCamera* camera = &map_state->camera;
        camera->pos[0] = camera->interp.from[0] = camera->interp.to[0] = camera->interp.pos[0]
            = (float)map_state->size[0] - (float)HALF_SCREEN_WIDTH;
        camera->pos[1] = camera->interp.from[1] = camera->interp.to[1] = camera->interp.pos[1] = HALF_SCREEN_HEIGHT;
    }

    play_generic_track(GTS_MAIN, map_state->track, PLAY_LOOPING, offset);
}

static void end() {
    SDL_free((void*)map_state->title);
    SDL_free((void*)map_state->track);
    destroy_surface(map_state->surface);
    destroy_tilemap(map_state->tilemap);
    FreeTinyD(map_state->points);
    FreeTinyD(map_state->path);
    FreeTinyD(map_state->player.bubbles);
    SDL_free(map_state);
}

static void tick() {
    if (topui() == NULL && kb_pressed(KB_PAUSE)) {
        create_ui(UI_PAUSE, NULL);
        if (!is_connected())
            return;
    }

    map_state->label.y += map_state->label.y_speed;
    map_state->label.y_speed += 0.4f;
    if (map_state->label.y > 16.f) {
        map_state->label.y -= map_state->label.y_speed;
        map_state->label.y_speed = (map_state->label.y_speed * -0.5f) - 1.f;
    }

    Bool can_move = FALSE;
    if (map_state->path != NULL) {
        MapPlayer* player = &map_state->player;

        if (map_state->current_node < TinyDLength(map_state->path)) {
            if (player->appear_speed <= 1) {
                if (kb_pressed(KB_JUMP) || kb_pressed(KB_FIRE) || kb_pressed(KB_UI_ENTER))
                    player->appear_speed += 5;
            } else if (player->speed < 12.5f) {
                player->speed += player->speed;
                if (player->speed > 12.5f)
                    player->speed = 12.5f;
            }

            const MapPathNode* node = &map_state->path[map_state->current_node];
            const float nx = (float)node->pos[0], ny = (float)node->pos[1];

            const float dir = SDL_atan2f(ny - player->pos[1], nx - player->pos[0]);
            player->pos[0] += SDL_cosf(dir) * player->speed;
            player->pos[1] += SDL_sinf(dir) * player->speed;

            if (SDL_sqrtf(SDL_powf(nx - player->pos[0], 2.f) + SDL_powf(ny - player->pos[1], 2.f)) < player->speed) {
                player->pos[0] = nx;
                player->pos[1] = ny;
                ++map_state->current_node;
            }

            player->frame += player->swimming ? 0.14f : 0.48f;

            const Sint32 xmax = map_state->size[0] - HALF_SCREEN_WIDTH, ymax = map_state->size[1] - HALF_SCREEN_HEIGHT;
            map_state->camera.pos[0] = SDL_clamp(player->pos[0], HALF_SCREEN_WIDTH, xmax);
            map_state->camera.pos[1] = SDL_clamp(player->pos[1], HALF_SCREEN_HEIGHT, ymax);
        } else {
            if (map_state->ambush > 0 && map_state->ambush <= 33) {
                switch (map_state->ambush++) {
                default:
                    break;

                case 1:
                case 18:
                    play_generic_sound("ambush", 0);
                    break;

                case 33: {
                    const WorldContext* wctx = worldcontext();
                    play_generic_sound(get_character_voice(wctx->players[wctx->winner].character, PV_PANIC), 0);
                    break;
                }
                }
            }

            MapTransition* transition = &map_state->transition;
            if (transition->frame <= 0.f) {
                can_move = TRUE;

                if (kb_pressed(KB_JUMP) && is_leader()) {
                    melt_generic_track(GTS_MAIN);
                    transition->frame += 1.f;
                }
            } else {
                transition->frame += 1.f;

                if (transition->frame == 25.f)
                    play_generic_sound("ui/enter", 0);

                if (transition->frame >= 70.f) {
                    const Level* level = get_level_key(map_state->level);
                    if (level == NULL) {
                        WorldContext wctx = *worldcontext();
                        ++wctx.level;
                        jump_to_world(&wctx, FALSE);
                    } else {
                        GameContext gctx = init_game_context(worldcontext(), map_state->level);
                        jump_to_game(&gctx, FALSE);
                    }
                }
            }

            player->frame += player->swimming ? 0.14f : 0.12f;
        }

        if ((player->pos[0] - 8.f) < (float)map_state->water[2] && (player->pos[0] + 8.f) > (float)map_state->water[0]
            && (player->pos[1] - 29.f) < (float)map_state->water[3]
            && (player->pos[1] + 1.f) > (float)map_state->water[1])
        {
            if (!player->swimming) {
                player->frame = 0.f;
                player->swimming = TRUE;
            }
        } else if (player->swimming) {
            player->frame = 0.f;
            player->swimming = FALSE;
        }

        if (player->swimming) {
            if (player->bubbles == NULL)
                player->bubbles = MakeTinyDPro(sizeof(MapBubble), sizeof(MapBubble));

            MapBubble bubble = {0};
            bubble.pos[0] = bubble.interp.from[0] = bubble.interp.to[0] = bubble.interp.pos[0] = player->pos[0] - 1.f;
            bubble.pos[1] = bubble.interp.from[1] = bubble.interp.to[1] = bubble.interp.pos[1]
                = player->pos[1] - 10.f + (float)SDL_rand(8) - (float)SDL_rand(8);
            const float dir = SDL_PI_F + ((float)SDL_rand(8) * (SDL_PI_F / 16.f));
            bubble.speed[0] = SDL_cosf(dir) * 1.375f;
            bubble.speed[1] = SDL_sinf(dir) * 1.375f;

            player->bubbles = TinyDPush(player->bubbles, &bubble);
        }

        size_t i = TinyDLength(player->bubbles);
        while (i-- > 0) {
            MapBubble* bubble = &player->bubbles[i];
            if (++bubble->frame >= 11) {
                player->bubbles = TinyDErase(player->bubbles, i);
                continue;
            }

            bubble->pos[0] += bubble->speed[0];
            bubble->pos[1] += bubble->speed[1];
            bubble->speed[1] += 0.2f;
        }

        Uint8 cross = 0;
        for (size_t i = 0, n = TinyDLength(map_state->points); i < n; i++) {
            MapPoint* point = &map_state->points[i];
            if (point->cross && ++cross > worldcontext()->level)
                break;

            if (point->appear <= 0) {
                if ((player->pos[0] - 8.f) < (float)(point->pos[0] + 7)
                    && (player->pos[0] + 8.f) > (float)(point->pos[0] - 7)
                    && (player->pos[1] - 30.f) < (float)(point->pos[1] + 7)
                    && player->pos[1] > (float)(point->pos[1] - 6))
                {
                    point->appear += player->appear_speed;

                    size_t j = i;
                    while (j-- > 0) {
                        if (map_state->points[j].appear > 0)
                            break;

                        map_state->points[j].appear += player->appear_speed;
                    }
                }
            } else if (point->appear <= 20) {
                point->appear += player->appear_speed;
            }
        }
    } else {
        MapTransition* transition = &map_state->transition;
        if (transition->frame <= 0.f) {
            if (kb_pressed(KB_JUMP) || kb_pressed(KB_PAUSE))
                transition->frame += 1.f;
        } else {
            transition->frame += 1.f;
            if (transition->frame > 101.f)
                set_screen(SCR_MENU, NULL, 0);
        }
    }

    if (can_move) {
        const float xmove = map_state->camera.pos[0] + (((float)kb_down(KB_RIGHT) - (float)kb_down(KB_LEFT)) * 5.f),
                    ymove = map_state->camera.pos[1] + (((float)kb_down(KB_DOWN) - (float)kb_down(KB_UP)) * 5.f);
        const float xmax = (float)map_state->size[0] - (float)HALF_SCREEN_WIDTH,
                    ymax = (float)map_state->size[1] - (float)HALF_SCREEN_HEIGHT;
        map_state->camera.pos[0] = SDL_clamp(xmove, HALF_SCREEN_WIDTH, xmax);
        map_state->camera.pos[1] = SDL_clamp(ymove, HALF_SCREEN_HEIGHT, ymax);
    }
}

static void pre_interp() {
    MapLabel* label = &map_state->label;
    MapPlayer* player = &map_state->player;
    MapCamera* camera = &map_state->camera;
    MapTransition* transition = &map_state->transition;

    const int fps = get_framerate();
    if (fps > 0 && fps <= TICKRATE) {
        label->interp.from = label->interp.to = label->interp.y = label->y;

        player->interp.from[0] = player->interp.to[0] = player->interp.pos[0] = player->pos[0];
        player->interp.from[1] = player->interp.to[1] = player->interp.pos[1] = player->pos[1];
        for (size_t i = 0, n = TinyDLength(player->bubbles); i < n; i++) {
            MapBubble* bubble = &player->bubbles[i];
            bubble->interp.from[0] = bubble->interp.to[0] = bubble->interp.pos[0] = bubble->pos[0];
            bubble->interp.from[1] = bubble->interp.to[1] = bubble->interp.pos[1] = bubble->pos[1];
        }

        camera->interp.from[0] = camera->interp.to[0] = camera->interp.pos[0] = camera->pos[0];
        camera->interp.from[1] = camera->interp.to[1] = camera->interp.pos[1] = camera->pos[1];

        transition->interp.from = transition->interp.to = transition->interp.frame = transition->frame;

        return;
    }

    label->interp.from = label->interp.to;
    label->interp.to = label->y;

    player->interp.from[0] = player->interp.to[0];
    player->interp.from[1] = player->interp.to[1];
    player->interp.to[0] = player->pos[0];
    player->interp.to[1] = player->pos[1];
    for (size_t i = 0, n = TinyDLength(player->bubbles); i < n; i++) {
        MapBubble* bubble = &player->bubbles[i];
        bubble->interp.from[0] = bubble->interp.to[0];
        bubble->interp.from[1] = bubble->interp.to[1];
        bubble->interp.to[0] = bubble->pos[0];
        bubble->interp.to[1] = bubble->pos[1];
    }

    camera->interp.from[0] = camera->interp.to[0];
    camera->interp.from[1] = camera->interp.to[1];
    camera->interp.to[0] = camera->pos[0];
    camera->interp.to[1] = camera->pos[1];

    transition->interp.from = transition->interp.to;
    transition->interp.to = transition->frame;
}

static void interp() {
    const int fps = get_framerate();
    if (fps > 0 && fps <= TICKRATE)
        return;

    MapLabel* label = &map_state->label;
    MapPlayer* player = &map_state->player;
    MapCamera* camera = &map_state->camera;
    MapTransition* transition = &map_state->transition;
    const float t = pendingticks();

    label->interp.y = glm_lerp(label->interp.from, label->interp.to, t);

    player->interp.pos[0] = glm_lerp(player->interp.from[0], player->interp.to[0], t);
    player->interp.pos[1] = glm_lerp(player->interp.from[1], player->interp.to[1], t);
    for (size_t i = 0, n = TinyDLength(player->bubbles); i < n; i++) {
        MapBubble* bubble = &player->bubbles[i];
        bubble->interp.pos[0] = glm_lerp(bubble->interp.from[0], bubble->interp.to[0], t);
        bubble->interp.pos[1] = glm_lerp(bubble->interp.from[1], bubble->interp.to[1], t);
    }

    camera->interp.pos[0] = glm_lerp(camera->interp.from[0], camera->interp.to[0], t);
    camera->interp.pos[1] = glm_lerp(camera->interp.from[1], camera->interp.to[1], t);

    transition->interp.frame = glm_lerp(transition->interp.from, transition->interp.to, t);
}

static void draw_ui() {
    push_surface(map_state->surface);
    clear_depth(1.f);

    batch_filter(FALSE);

    // World
    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    const float cx = (float)(int)map_state->camera.interp.pos[0], cy = (float)(int)map_state->camera.interp.pos[1];
    glm_ortho(cx - HALF_SCREEN_WIDTH, cx + HALF_SCREEN_WIDTH, cy - HALF_SCREEN_HEIGHT,
        cy - (SCREEN_HEIGHT + HALF_SCREEN_HEIGHT), -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    draw_tilemap(map_state->tilemap);

    if (map_state->path != NULL) {
        batch_reset();

        const WorldContext* wctx = worldcontext();

        const Uint32 t = (Uint32)(totalticks() * 0.5f);
        Uint8 cross = 0;
        for (size_t i = 0, n = TinyDLength(map_state->points); i < n; i++) {
            const MapPoint* point = &map_state->points[i];
            if (point->cross && ++cross > worldcontext()->level)
                break;
            if (point->appear <= 20)
                continue;

            batch_pos(B_XYZ(point->pos[0], point->pos[1], 10.f));
            batch_sprite(point->cross ? fmt("ui/map/cross/%u", t % 11) : fmt("ui/map/point/%u", t % 6));
        }

        const MapPlayer* player = &map_state->player;
        for (size_t i = 0; i < TinyDLength(player->bubbles); i++) {
            const MapBubble* bubble = &player->bubbles[i];

            batch_pos(B_XYZ((int)bubble->interp.pos[0], (int)bubble->interp.pos[1], 1.f));
            const float scale = 1.f - ((float)bubble->frame / 11.f);
            batch_scale(B_SIZE(scale));
            batch_sprite("ui/map/bubble");
        }

        const float px = (float)(int)player->interp.pos[0], py = (float)(int)player->interp.pos[1];
        batch_pos(B_XY(px, py));
        batch_scale(B_SIZE(0.5f));

        const WorldPlayerContext* pctx = &wctx->players[wctx->winner];
        batch_sprite(get_character_sprite(pctx->character, pctx->powerup,
            player->swimming ? ((player->frame < 6.f) ? (PF_SWIM1 + (int)player->frame)
                                                      : (PF_SWIM7 + (int)SDL_fmodf(player->frame, 2.f)))
                             : (PF_WALK1 + (int)SDL_fmodf(player->frame, 3.f))));

        batch_scale(B_SIZE(1.f));

        if (map_state->ambush > 1 && map_state->current_node >= TinyDLength(map_state->path)) {
            batch_pos(B_XY(px + 24.f, py - SDL_fabsf(SDL_sinf(totalticks() * 12.f * (SDL_PI_F / 180.f)) * 10.f)));
            batch_sprite("ui/map/bro");
        }
    }

    // HUD
    batch_test_depth(FALSE);
    batch_write_depth(FALSE);

    glm_ortho(0.f, SCREEN_WIDTH, 0.f, -SCREEN_HEIGHT, -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, (int)map_state->label.interp.y));

    batch_sprite(map_state->title);
    if (map_state->path == NULL) {
        const Sprite* tspr = get_sprite(map_state->title);
        const float tb = (tspr == NULL) ? 0.f : (tspr->size[1] - tspr->offset[1]);
        batch_pos(B_XY(HALF_SCREEN_WIDTH, (int)map_state->label.interp.y + tb));
        batch_sprite(fmt("ui/map/world/completed/%u", (Uint32)(totalticks() * 0.5f) % 16));
    }

    batch_pos(B_SCREEN);
    batch_sprite("ui/map/logo");

    // Transition
    const float transition = map_state->transition.interp.frame;
    if (transition <= 0.f) {
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
    } else if (map_state->path == NULL) {
        batch_pos(B_ORIGIN);
        batch_color(B_RGBA(0, 0, 0, (transition > 101.f) ? 255 : ((transition / 101.f) * 255.f)));
        batch_rectangle(NULL, B_SCREEN_SIZE);
    } else if (transition > 25.f) {
        if (transition >= 70.f) {
            batch_pos(B_ORIGIN);
            batch_color(B_BLACK);
            batch_rectangle(NULL, B_SCREEN_SIZE);
        } else {
            clear_stencil(0);

            batch_write_color(FALSE, FALSE, FALSE, FALSE);
            batch_test_stencil(TRUE);
            batch_stencil_func(STF_ALWAYS, 1, 1);
            batch_stencil_op(STO_REPLACE, STO_REPLACE, STO_REPLACE);
            batch_pos(B_HALF_SCREEN);
            batch_color(B_WHITE);
            batch_circle(NULL, (1.f - ((transition - 25.f) / 45.f)) * 400.f);
            batch_write_color(TRUE, TRUE, TRUE, TRUE);

            batch_stencil_func(STF_GREATER, 1, 1);
            batch_stencil_op(STO_KEEP, STO_KEEP, STO_KEEP);
            batch_pos(B_ORIGIN);
            batch_color(B_BLACK);
            batch_rectangle(NULL, B_SCREEN_SIZE);
            batch_test_stencil(FALSE);
        }
    }

    batch_write_depth(TRUE);
    batch_test_depth(TRUE);
    batch_filter(TRUE);

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
    .pre_interp = pre_interp,
    .interp = interp,
    .draw_ui = draw_ui,
};
