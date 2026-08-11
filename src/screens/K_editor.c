#include <SDL3/SDL_platform_defines.h>

#include "K_interface.h"

#ifndef SDL_PLATFORM_EMSCRIPTEN
//
//
//

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>

#include <dcimgui.h>
#include <dcimgui_impl_opengl3.h>
#include <dcimgui_impl_sdl3.h>

#include "K_game.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_video.h"

typedef Uint8 AsyncID;
enum {
    ASYNC_OPEN,
    ASYNC_BLUEPRINT,
    ASYNC_SIZE,
};

typedef Uint8 DefType;
enum {
    DEFT_INVALID,
    DEFT_BACKGROUND,
    DEFT_TILE,
    DEFT_ACTOR,
};

typedef struct {
    Bool available;
    const char* ptr;
} EditorAsync;

typedef struct {
    Bool hidden;
    int default_value;
    size_t index;
    const char* name;
} EditorDefValue;

typedef struct {
    Bool hidden, default_value;
    size_t index;
    const char* name;
} EditorDefFlag;

typedef struct {
    DefType type;
    ActorType actor;

    Bool flip[2], tile[2], scalable;
    float max_scale[2], colors[4][4];
    int depth;

    TinyHash previous, next;
    const char *name, *sprite;

    EditorDefValue* values;
    EditorDefFlag* flags;
} EditorDef;

typedef struct {
    Bool spawn_once, spawn_singleplayer, spawn_multiplayer, flip[2], tile[2];
    int pos[3];
    float scale[2], colors[4][4], vel[2];

    int values[MAX_VALUES];
    unsigned int flags;

    TinyHash def_key;
} EditorMarker;

typedef struct EditorFolder {
    const char* name;
    struct EditorFolder* folders;
    TinyHash* defs;
} EditorFolder;

typedef struct {
    Bool has_scalable, has_highlighted, has_selected;
    Uint16 grid_size;
    Sint32 pos[2];
    size_t scalable, highlighted, selected;
} EditorCursor;

typedef struct {
    Bool show_grid;
    float pos[2], hold[2], zoom;
} EditorCamera;

typedef struct {
    char label[256];
    char tracks[GSTR_TRACK_END - GSTR_TRACK_START + 1][256];
    char warps[MAX_GAME_WARPS][256];
    char secrets[GSTR_SECRET_END - GSTR_SECRET_START + 1][256];

    Bool ambush;
    unsigned int flags;

    int size[2], bounds[4];
    int time;

    EditorMarker* markers;
} EditorLevel;

typedef struct {
    EditorCursor cursor;
    EditorCamera camera;
    EditorLevel level;

    EditorAsync async[ASYNC_SIZE];
    const char* error;
    Surface* blueprint;

    TinyMap defs;
    TinyHash def_key;

    EditorFolder* folders;
} Editor;

extern SDL_Window* WINDOW;

static Editor* editor = NULL;

static Bool has_async(AsyncID id) {
    return editor->async[id].available;
}

static const char* get_async(AsyncID id) {
    return editor->async[id].ptr;
}

static void set_async(AsyncID id, const char* string) {
    EditorAsync* async = &editor->async[id];
    SDL_free((void*)async->ptr);
    async->ptr = (string == NULL) ? NULL : SDL_strdup(string);
    async->available = TRUE;
}

static void clear_async(AsyncID id) {
    EditorAsync* async = &editor->async[id];
    SDL_free((void*)async->ptr);
    async->ptr = NULL;
    async->available = FALSE;
}

static void move_cursor(const Sint32 pos[2], Bool snap, Bool highlight) {
    EditorCursor* ecursor = &editor->cursor;
    if (ImGui_GetIO()->WantCaptureMouse || pos[0] < SDL_MIN_SINT16 || pos[1] < SDL_MIN_SINT16 || pos[0] > SDL_MAX_SINT16
        || pos[1] > SDL_MAX_SINT16)
    {
        ecursor->has_highlighted = FALSE;
        return;
    }

    if (highlight && !ecursor->has_scalable) {
        Sint32 ox = ecursor->pos[0], oy = ecursor->pos[1];
        if (pos[0] != ox || pos[1] != oy) {
            ecursor->has_highlighted = FALSE;

            Sint64 depth = SDL_MAX_SINT64;
            const EditorMarker* emarkers = editor->level.markers;
            const EditorDef* cdef = (EditorDef*)TinyMapGet(&editor->defs, editor->def_key);
            for (size_t i = 0, n = TinyDLength(emarkers); i < n; i++) {
                const EditorMarker* marker = &emarkers[i];
                if (marker->pos[2] > depth)
                    continue;

                const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
                if (def != NULL && cdef != NULL && def->type != cdef->type)
                    continue;

                float x1 = (float)marker->pos[0], y1 = (float)marker->pos[1], x2 = x1, y2 = y1;

                const Sprite* sprite = (marker->tile[0] || marker->tile[1]) ? NULL : get_sprite(def->sprite);
                if (sprite == NULL) {
                    x2 += marker->scale[0];
                    y2 += marker->scale[1];
                } else {
                    x1 -= sprite->offset[0];
                    y1 -= sprite->offset[1];
                    x2 = x1 + (sprite->size[0] * marker->scale[0]);
                    y2 = y1 + (sprite->size[1] * marker->scale[1]);
                }

                const float cx = (float)pos[0], cy = (float)pos[1];
                if (cx >= x1 && cx < x2 && cy >= y1 && cy < y2) {
                    ecursor->has_highlighted = TRUE;
                    ecursor->highlighted = i;
                    depth = marker->pos[2];
                }
            }
        }
    } else {
        ecursor->has_highlighted = FALSE;
    }

    if (snap) {
        const float gsize = ecursor->grid_size;
        ecursor->pos[0] = (Sint32)(SDL_floorf((float)pos[0] / gsize) * gsize);
        ecursor->pos[1] = (Sint32)(SDL_floorf((float)pos[1] / gsize) * gsize);
    } else {
        ecursor->pos[0] = pos[0];
        ecursor->pos[1] = pos[1];
    }
}

static void clear_level() {
    EditorLevel* level = &editor->level;

    // Cleanup
    FreeTinyD(level->markers);

    // Nullify
    SDL_zerop(level);
    level->size[0] = SCREEN_WIDTH;
    level->size[1] = SCREEN_HEIGHT;
    level->bounds[2] = SCREEN_WIDTH;
    level->bounds[3] = SCREEN_HEIGHT;
    level->time = -1;

    editor->cursor.has_scalable = editor->cursor.has_highlighted = editor->cursor.has_selected = FALSE;
}

static EditorMarker init_marker() {
    EditorMarker marker = {0};

    marker.scale[0] = marker.scale[1] = 1.f;
    for (size_t i = 0; i < SDL_arraysize(marker.colors); i++)
        for (size_t j = 0; j < SDL_arraysize(*marker.colors); j++)
            marker.colors[i][j] = 1.f;

    return marker;
}

static void open_level(const char* filename) {
    yyjson_read_err error = {0};
    yyjson_doc* json
        = yyjson_read_file(filename, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, &error);
    if (json == NULL) {
        editor->error = error.msg;
        return;
    }

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        editor->error = "edit_invalid_level";
        yyjson_doc_free(json);
        return;
    }

    clear_level();

    EditorLevel* elevel = &editor->level;

    yyjson_val* jval = yyjson_obj_get(root, "label");
    if (yyjson_is_str(jval))
        SDL_strlcpy(elevel->label, yyjson_get_str(jval), sizeof(elevel->label));

    jval = yyjson_obj_get(root, "tracks");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n && i < SDL_arraysize(elevel->tracks); i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (yyjson_is_str(jval2))
            SDL_strlcpy(elevel->tracks[i], yyjson_get_str(jval2), sizeof(elevel->tracks[i]));
    }

    jval = yyjson_obj_get(root, "warps");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n && i < SDL_arraysize(elevel->warps); i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (yyjson_is_str(jval2))
            SDL_strlcpy(elevel->warps[i], yyjson_get_str(jval2), sizeof(elevel->warps[i]));
    }

    jval = yyjson_obj_get(root, "secrets");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n && i < SDL_arraysize(elevel->secrets); i++) {
        yyjson_val* jval2 = yyjson_arr_get(jval, i);
        if (yyjson_is_str(jval2))
            SDL_strlcpy(elevel->secrets[i], yyjson_get_str(jval2), sizeof(elevel->secrets[i]));
    }

    elevel->ambush = yyjson_get_bool(yyjson_obj_get(root, "ambush"));
    elevel->flags |= yyjson_get_bool(yyjson_obj_get(root, "hardcore")) * GF_HARDCORE;
    elevel->flags |= yyjson_get_bool(yyjson_obj_get(root, "lost_map")) * GF_LOST_MAP;
    elevel->flags |= yyjson_get_bool(yyjson_obj_get(root, "funny_tanks")) * GF_FUNNY_TANKS;

    jval = yyjson_obj_get(root, "size");
    if (yyjson_is_arr(jval)) {
        elevel->size[0] = (int)yyjson_get_uint(yyjson_arr_get(jval, 0));
        elevel->size[1] = (int)yyjson_get_uint(yyjson_arr_get(jval, 1));
    }

    jval = yyjson_obj_get(root, "bounds");
    if (yyjson_is_arr(jval)) {
        elevel->bounds[0] = (int)yyjson_get_sint(yyjson_arr_get(jval, 0));
        elevel->bounds[1] = (int)yyjson_get_sint(yyjson_arr_get(jval, 1));
        elevel->bounds[2] = (int)yyjson_get_sint(yyjson_arr_get(jval, 2));
        elevel->bounds[3] = (int)yyjson_get_sint(yyjson_arr_get(jval, 3));
    }

    jval = yyjson_obj_get(root, "time");
    if (yyjson_is_int(jval))
        elevel->time = (int)yyjson_get_sint(jval);

    jval = yyjson_obj_get(root, "backdrops");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n; i++) {
        yyjson_val* jmarker = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jmarker))
            continue;

        const char* def_name = yyjson_get_str(yyjson_obj_get(jmarker, "def"));
        const TinyHash def_key = StHashStr(def_name);
        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, def_key);
        if (def == NULL || (def->type != DEFT_BACKGROUND && def->type != DEFT_TILE)) {
            WARN("Backdrop marker %zu has invalid def \"%s\" and will be removed", i, def_name);
            continue;
        }

        EditorMarker marker = init_marker();
        marker.def_key = def_key;

        yyjson_val* jmval = yyjson_obj_get(jmarker, "pos");
        marker.pos[0] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 0));
        marker.pos[1] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 1));
        marker.pos[2] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 2));

        jmval = yyjson_obj_get(jmarker, "tile");
        marker.tile[0] = yyjson_get_bool(yyjson_arr_get(jmval, 0));
        marker.tile[1] = yyjson_get_bool(yyjson_arr_get(jmval, 1));

        jmval = yyjson_obj_get(jmarker, "size");
        if (yyjson_is_arr(jmval)) {
            marker.scale[0] = (float)yyjson_get_sint(yyjson_arr_get(jmval, 0));
            marker.scale[1] = (float)yyjson_get_sint(yyjson_arr_get(jmval, 1));
            const Sprite* sprite = (marker.tile[0] || marker.tile[1]) ? NULL : get_sprite(def->sprite);
            if (sprite != NULL) {
                marker.scale[0] /= sprite->size[0];
                marker.scale[1] /= sprite->size[1];
            }
        }

        jmval = yyjson_obj_get(jmarker, "flip");
        marker.flip[0] = yyjson_get_bool(yyjson_arr_get(jmval, 0));
        marker.flip[1] = yyjson_get_bool(yyjson_arr_get(jmval, 1));

        jmval = yyjson_obj_get(jmarker, "colors");
        if (yyjson_is_arr(jmval)) {
            for (size_t j = 0, n2 = yyjson_arr_size(jmval); j < n2 && j < SDL_arraysize(marker.colors); j++) {
                yyjson_val* jmval2 = yyjson_arr_get(jmval, j);
                for (size_t k = 0, n3 = yyjson_arr_size(jmval2); k < n3 && k < SDL_arraysize(*marker.colors); k++)
                    marker.colors[j][k] = (float)yyjson_get_uint(yyjson_arr_get(jmval2, k)) / 255.f;
            }
        }

        if (elevel->markers == NULL)
            elevel->markers = MakeTinyDPro(1, sizeof(*elevel->markers));
        elevel->markers = TinyDPush(elevel->markers, &marker);
    }

    jval = yyjson_obj_get(root, "actors");
    for (size_t i = 0, n = yyjson_arr_size(jval); i < n; i++) {
        yyjson_val* jmarker = yyjson_arr_get(jval, i);
        if (!yyjson_is_obj(jmarker))
            continue;

        const char* def_name = yyjson_get_str(yyjson_obj_get(jmarker, "def"));
        const TinyHash def_key = StHashStr(def_name);
        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, def_key);
        if (def == NULL || def->type != DEFT_ACTOR) {
            WARN("Actor marker %zu has invalid def \"%s\" and will be removed", i, def_name);
            continue;
        }

        EditorMarker marker = init_marker();
        marker.def_key = def_key;

        marker.spawn_once = yyjson_get_bool(yyjson_obj_get(jmarker, "once"));
        marker.spawn_singleplayer = yyjson_get_bool(yyjson_obj_get(jmarker, "singleplayer"));
        marker.spawn_multiplayer = yyjson_get_bool(yyjson_obj_get(jmarker, "multiplayer"));

        yyjson_val* jmval = yyjson_obj_get(jmarker, "pos");
        marker.pos[0] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 0));
        marker.pos[1] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 1));
        marker.pos[2] = (int)yyjson_get_sint(yyjson_arr_get(jmval, 2));

        jmval = yyjson_obj_get(jmarker, "scale");
        if (yyjson_is_arr(jmval)) {
            marker.scale[0] = Fx2Float(yyjson_get_sint(yyjson_arr_get(jmval, 0)));
            marker.scale[1] = Fx2Float(yyjson_get_sint(yyjson_arr_get(jmval, 1)));
        }

        jmval = yyjson_obj_get(jmarker, "values");
        for (size_t j = 0, n2 = yyjson_arr_size(jmval); j < n2; j++) {
            yyjson_val* jmval2 = yyjson_arr_get(jmval, j);
            const size_t index = yyjson_get_uint(yyjson_arr_get(jmval2, 0));
            if (index < MAX_VALUES)
                marker.values[index] = yyjson_get_int(yyjson_arr_get(jmval2, 1));
        }

        marker.flags = yyjson_get_uint(yyjson_obj_get(jmarker, "flags"));

        if (elevel->markers == NULL)
            elevel->markers = MakeTinyDPro(1, sizeof(*elevel->markers));
        elevel->markers = TinyDPush(elevel->markers, &marker);
    }

    yyjson_doc_free(json);
}

static void open_level_async(void* userdata, const char* const* files, int filter) {
    (void)userdata;
    (void)filter;

    ASSUME(files && *files, "No file to open");
    set_async(ASYNC_OPEN, *files);
}

static void open_level_dialog() {
    SDL_DialogFileFilter filter = {0};
    filter.name = "JSON file";
    filter.pattern = "json";
    SDL_ShowOpenFileDialog(open_level_async, NULL, WINDOW, &filter, 1, NULL, FALSE);
}

typedef struct {
    SolidFlags flags;
    int pos[2];
} EditorCollisionCell;

typedef struct {
    int min_pos[2], max_pos[2];
    unsigned int cell_size[2];
    EditorCollisionCell* cells;
} EditorCollisionMap;

static void save_level(const char* filename) {
    if (filename == NULL)
        return;

    yyjson_mut_doc* json = yyjson_mut_doc_new(NULL);

    yyjson_mut_val* root = yyjson_mut_obj(json);
    yyjson_mut_doc_set_root(json, root);

    const EditorLevel* elevel = &editor->level;

    if (elevel->label[0] != '\0')
        yyjson_mut_obj_add_strcpy(json, root, "label", elevel->label);

    for (size_t i = 0; i < SDL_arraysize(elevel->tracks); i++) {
        if (elevel->tracks[i][0] == '\0')
            continue;

        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "tracks");
        for (size_t j = 0; j < SDL_arraysize(elevel->tracks); j++)
            if (elevel->tracks[j][0] == '\0')
                yyjson_mut_arr_add_null(json, jval);
            else
                yyjson_mut_arr_add_strcpy(json, jval, elevel->tracks[j]);

        break;
    }

    for (size_t i = 0; i < SDL_arraysize(elevel->warps); i++) {
        if (elevel->warps[i][0] == '\0')
            continue;

        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "warps");
        for (size_t j = 0; j < SDL_arraysize(elevel->warps); j++)
            if (elevel->warps[j][0] == '\0')
                yyjson_mut_arr_add_null(json, jval);
            else
                yyjson_mut_arr_add_strcpy(json, jval, elevel->warps[j]);

        break;
    }

    for (size_t i = 0; i < SDL_arraysize(elevel->secrets); i++) {
        if (elevel->secrets[i][0] == '\0')
            continue;

        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "secrets");
        for (size_t j = 0; j < SDL_arraysize(elevel->secrets); j++)
            if (elevel->secrets[j][0] == '\0')
                yyjson_mut_arr_add_null(json, jval);
            else
                yyjson_mut_arr_add_strcpy(json, jval, elevel->secrets[j]);

        break;
    }

    if (elevel->flags & GF_HARDCORE)
        yyjson_mut_obj_add_bool(json, root, "hardcore", TRUE);
    if (elevel->flags & GF_LOST_MAP)
        yyjson_mut_obj_add_bool(json, root, "lost_map", TRUE);
    if (elevel->flags & GF_FUNNY_TANKS)
        yyjson_mut_obj_add_bool(json, root, "funny_tanks", TRUE);
    if (elevel->ambush)
        yyjson_mut_obj_add_bool(json, root, "ambush", TRUE);

    if (elevel->size[0] != SCREEN_WIDTH || elevel->size[1] != SCREEN_HEIGHT) {
        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "size");
        yyjson_mut_arr_add_uint(json, jval, elevel->size[0]);
        yyjson_mut_arr_add_uint(json, jval, elevel->size[1]);
    }

    if (elevel->bounds[0] != 0 || elevel->bounds[1] != 0 || elevel->bounds[2] != SCREEN_WIDTH
        || elevel->bounds[3] != SCREEN_HEIGHT)
    {
        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "bounds");
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[0]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[1]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[2]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[3]);
    }

    if (elevel->time != -1)
        yyjson_mut_obj_add_sint(json, root, "time", elevel->time);

    yyjson_mut_val *jbackdrops = yyjson_mut_obj_add_arr(json, root, "backdrops"),
                   *jcollisions = yyjson_mut_obj_add_arr(json, root, "collisions"),
                   *jactors = yyjson_mut_obj_add_arr(json, root, "actors");
    TinyMap collisions = {0};
    for (size_t i = 0, n = TinyDLength(elevel->markers); i < n; i++) {
        const EditorMarker* marker = &elevel->markers[i];

        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
        if (def == NULL)
            continue;

        switch (def->type) {
        default: {
            WARN("Marker %zu has invalid def \"%s\" and will be excluded", i, def->name);
            break;
        }

        case DEFT_BACKGROUND:
        case DEFT_TILE: {
            yyjson_mut_val* jmarker = yyjson_mut_arr_add_obj(json, jbackdrops);
            yyjson_mut_obj_add_strcpy(json, jmarker, "def", def->name);

            if (def->sprite != NULL)
                yyjson_mut_obj_add_strcpy(json, jmarker, "sprite", def->sprite);

            if (marker->pos[0] != 0 || marker->pos[1] != 0 || marker->pos[2] != 0) {
                yyjson_mut_val* jpos = yyjson_mut_obj_add_arr(json, jmarker, "pos");
                yyjson_mut_arr_add_sint(json, jpos, marker->pos[0]);
                yyjson_mut_arr_add_sint(json, jpos, marker->pos[1]);
                if (marker->pos[2] != 0)
                    yyjson_mut_arr_add_sint(json, jpos, marker->pos[2]);
            }

            const Sprite* sprite = (marker->tile[0] || marker->tile[1]) ? NULL : get_sprite(def->sprite);
            if (marker->scale[0] != 1.f || marker->scale[1] != 1.f) {
                yyjson_mut_val* jsize = yyjson_mut_obj_add_arr(json, jmarker, "size");
                if (sprite == NULL) {
                    yyjson_mut_arr_add_sint(json, jsize, (Sint64)marker->scale[0]);
                    yyjson_mut_arr_add_sint(json, jsize, (Sint64)marker->scale[1]);
                } else {
                    yyjson_mut_arr_add_sint(json, jsize, (Sint64)(sprite->size[0] * marker->scale[0]));
                    yyjson_mut_arr_add_sint(json, jsize, (Sint64)(sprite->size[1] * marker->scale[1]));
                }
            }

            if (marker->flip[0] || marker->flip[1]) {
                yyjson_mut_val* jflip = yyjson_mut_obj_add_arr(json, jmarker, "flip");
                yyjson_mut_arr_add_bool(json, jflip, marker->flip[0]);
                yyjson_mut_arr_add_bool(json, jflip, marker->flip[1]);
            }

            if (marker->tile[0] || marker->tile[1]) {
                yyjson_mut_val* jtile = yyjson_mut_obj_add_arr(json, jmarker, "tile");
                yyjson_mut_arr_add_bool(json, jtile, marker->tile[0]);
                yyjson_mut_arr_add_bool(json, jtile, marker->tile[1]);
            }

            for (size_t j = 0; j < SDL_arraysize(marker->colors); j++) {
                for (size_t k = 0; k < SDL_arraysize(*marker->colors); k++) {
                    if (marker->colors[j][k] == 1.f)
                        continue;

                    yyjson_mut_val* jcolors = yyjson_mut_obj_add_arr(json, jmarker, "colors");
                    for (size_t l = 0; l < SDL_arraysize(marker->colors); l++) {
                        yyjson_mut_val* jcol = yyjson_mut_arr_add_arr(json, jcolors);
                        for (size_t m = 0; m < SDL_arraysize(*marker->colors); m++)
                            yyjson_mut_arr_add_uint(json, jcol, (Uint8)(marker->colors[l][m] * 255.f));
                    }

                    goto sl_jcolors_done;
                }
            }
        sl_jcolors_done:

            if (def->type != DEFT_TILE)
                break;

            float tile_width = marker->scale[0], tile_height = marker->scale[1];
            if (sprite != NULL) {
                tile_width *= sprite->size[0];
                tile_height *= sprite->size[1];
            }

            float tile_xoffset = (float)marker->pos[0], tile_yoffset = (float)marker->pos[1];
            if (sprite != NULL) {
                tile_xoffset -= sprite->offset[0];
                tile_yoffset -= sprite->offset[1];
            }
            while (tile_xoffset >= tile_width)
                tile_xoffset -= tile_width;
            while (tile_xoffset < 0.f)
                tile_xoffset += tile_width;
            while (tile_yoffset >= tile_height)
                tile_yoffset -= tile_height;
            while (tile_yoffset < 0.f)
                tile_yoffset += tile_height;

            const TinyHash key = StHashStr(
                fmt("%ix%i %i,%i", (int)tile_width, (int)tile_height, (int)tile_xoffset, (int)tile_yoffset));
            EditorCollisionMap* cmap = (EditorCollisionMap*)TinyMapGet(&collisions, key);
            if (cmap == NULL) {
                TinyBucket* bucket = TinyMapPut(&collisions, key, &(EditorCollisionMap){0}, sizeof(EditorCollisionMap));
                cmap = bucket->data;

                cmap->min_pos[0] = cmap->min_pos[1] = SDL_MAX_SINT16;
                cmap->max_pos[0] = cmap->max_pos[1] = SDL_MIN_SINT16;
                cmap->cell_size[0] = (unsigned int)tile_width;
                cmap->cell_size[1] = (unsigned int)tile_height;
            }

            cmap->min_pos[0] = SDL_min(marker->pos[0], cmap->min_pos[0]);
            cmap->min_pos[1] = SDL_min(marker->pos[1], cmap->min_pos[1]);
            cmap->max_pos[0] = SDL_max(marker->pos[0], cmap->max_pos[0]);
            cmap->max_pos[1] = SDL_max(marker->pos[1], cmap->max_pos[1]);

            EditorCollisionCell ccell = {0};
            ccell.pos[0] = marker->pos[0];
            ccell.pos[1] = marker->pos[1];
            ccell.flags = SOL_SOLID;

            if (cmap->cells == NULL)
                cmap->cells = MakeTinyDPro(1, sizeof(*cmap->cells));
            cmap->cells = TinyDPush(cmap->cells, &ccell);

            break;
        }

        case DEFT_ACTOR: {
            yyjson_mut_val* jmarker = yyjson_mut_arr_add_obj(json, jactors);
            yyjson_mut_obj_add_strcpy(json, jmarker, "def", def->name);
            yyjson_mut_obj_add_uint(json, jmarker, "id", def->actor);

            if (marker->spawn_once)
                yyjson_mut_obj_add_bool(json, jmarker, "once", TRUE);
            if (marker->spawn_singleplayer)
                yyjson_mut_obj_add_bool(json, jmarker, "singleplayer", TRUE);
            if (marker->spawn_multiplayer)
                yyjson_mut_obj_add_bool(json, jmarker, "multiplayer", TRUE);

            if (marker->pos[0] != 0 || marker->pos[1] != 0 || marker->pos[2] != 0) {
                yyjson_mut_val* jpos = yyjson_mut_obj_add_arr(json, jmarker, "pos");
                yyjson_mut_arr_add_sint(json, jpos, marker->pos[0]);
                yyjson_mut_arr_add_sint(json, jpos, marker->pos[1]);
                if (marker->pos[2] != 0)
                    yyjson_mut_arr_add_sint(json, jpos, marker->pos[2]);
            }

            if (marker->scale[0] != 1.f || marker->scale[1] != 1.f) {
                yyjson_mut_val* jscale = yyjson_mut_obj_add_arr(json, jmarker, "scale");
                yyjson_mut_arr_add_sint(json, jscale, Float2Fx(marker->scale[0]));
                yyjson_mut_arr_add_sint(json, jscale, Float2Fx(marker->scale[1]));
            }

            if (marker->vel[0] != 0.f || marker->vel[1] != 0.f) {
                yyjson_mut_val* jvel = yyjson_mut_obj_add_arr(json, jmarker, "vel");
                yyjson_mut_arr_add_sint(json, jvel, Float2Fx(marker->vel[0]));
                yyjson_mut_arr_add_sint(json, jvel, Float2Fx(marker->vel[1]));
            }

            const size_t num_values = TinyDLength(def->values);
            if (num_values > 0) {
                yyjson_mut_val* jvalues = yyjson_mut_obj_add_arr(json, jmarker, "values");
                for (size_t j = 0; j < num_values; j++) {
                    yyjson_mut_val* jvalue = yyjson_mut_arr_add_arr(json, jvalues);
                    const EditorDefValue* dvalue = &def->values[j];
                    yyjson_mut_arr_add_uint(json, jvalue, dvalue->index);
                    yyjson_mut_arr_add_sint(json, jvalue, marker->values[dvalue->index]);
                }
            }

            if (marker->flags > 0)
                yyjson_mut_obj_add_uint(json, jmarker, "flags", marker->flags);

            break;
        }
        }
    }

    TINY_MAP_FOREACH (&collisions, iter) {
        const EditorCollisionMap* cmap = iter.data;

        yyjson_mut_val* jcmap = yyjson_mut_arr_add_obj(json, jcollisions);

        if (cmap->min_pos[0] != 0 || cmap->min_pos[1] != 0) {
            yyjson_mut_val* jcmval = yyjson_mut_obj_add_arr(json, jcmap, "pos");
            yyjson_mut_arr_add_sint(json, jcmval, cmap->min_pos[0]);
            yyjson_mut_arr_add_sint(json, jcmval, cmap->min_pos[1]);
        }

        yyjson_mut_val* jcmval = yyjson_mut_obj_add_arr(json, jcmap, "size");
        yyjson_mut_arr_add_uint(json, jcmval,
            (Uint64)(((float)cmap->max_pos[0] - (float)cmap->min_pos[0]) / (float)cmap->cell_size[0]) + 1);
        yyjson_mut_arr_add_uint(json, jcmval,
            (Uint64)(((float)cmap->max_pos[1] - (float)cmap->min_pos[1]) / (float)cmap->cell_size[1]) + 1);

        jcmval = yyjson_mut_obj_add_arr(json, jcmap, "cell_size");
        yyjson_mut_arr_add_uint(json, jcmval, cmap->cell_size[0]);
        yyjson_mut_arr_add_uint(json, jcmval, cmap->cell_size[1]);

        jcmval = yyjson_mut_obj_add_arr(json, jcmap, "cells");
        for (size_t i = 0, n = TinyDLength(cmap->cells); i < n; i++) {
            const EditorCollisionCell* ccell = &cmap->cells[i];
            yyjson_mut_val* jccell = yyjson_mut_arr_add_arr(json, jcmval);
            yyjson_mut_arr_add_uint(
                json, jccell, (Uint64)((float)(ccell->pos[0] - cmap->min_pos[0]) / (float)cmap->cell_size[0]));
            yyjson_mut_arr_add_uint(
                json, jccell, (Uint64)((float)(ccell->pos[1] - cmap->min_pos[1]) / (float)cmap->cell_size[1]));
            yyjson_mut_arr_add_uint(json, jccell, ccell->flags);
        }

        FreeTinyD(cmap->cells);
    }

    FreeTinyMap(&collisions);

    const size_t len = SDL_strlen(filename);
    if (len < 5 || SDL_strcmp(filename + len - 5, ".json") != 0)
        filename = fmt("%s.json", filename);

    yyjson_write_err error = {0};
    if (!yyjson_mut_write_file(filename, json, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, NULL, &error))
        editor->error = error.msg;

    yyjson_mut_doc_free(json);
}

static void save_level_async(void* userdata, const char* const* files, int filter) {
    (void)userdata;
    (void)filter;

    ASSUME(files && *files, "No file to save");
    save_level(*files);
}

static void save_level_dialog() {
    SDL_DialogFileFilter filter = {0};
    filter.name = "JSON file";
    filter.pattern = "json";
    SDL_ShowSaveFileDialog(save_level_async, NULL, WINDOW, &filter, 1, NULL);
}

static void open_blueprint_async(void* userdata, const char* const* files, int filter) {
    (void)userdata;
    (void)files;
    (void)filter;

    set_async(ASYNC_BLUEPRINT, (files == NULL || *files == NULL) ? NULL : *files);
}

static void open_blueprint_dialog() {
    SDL_DialogFileFilter filter = {0};
    filter.name = "PNG file";
    filter.pattern = "png";
    SDL_ShowOpenFileDialog(open_blueprint_async, NULL, WINDOW, &filter, 1, NULL, FALSE);
}

static void nuke_def(void* ptr) {
    EditorDef* def = ptr;
    SDL_free((void*)def->name);
    SDL_free((void*)def->sprite);

    for (size_t i = 0, n = TinyDLength(def->values); i < n; i++)
        SDL_free((void*)def->values[i].name);
    FreeTinyD(def->values);

    for (size_t i = 0, n = TinyDLength(def->flags); i < n; i++)
        SDL_free((void*)def->flags[i].name);
    FreeTinyD(def->flags);
}

// @NOLINTBEGIN(misc-no-recursion)
static void create_folder(EditorFolder* parent, yyjson_val* jval) {
    Bool append = FALSE;
    size_t append_idx = 0;

    EditorFolder folder = {0};
    if (parent == NULL) {
        if (editor->folders != NULL)
            folder = *editor->folders;
    } else {
        const char* name = yyjson_get_str(yyjson_obj_get(jval, "name"));
        if (name != NULL) {
            for (size_t i = 0, n = TinyDLength(parent->folders); i < n; i++) {
                EditorFolder* ofolder = &parent->folders[i];
                if (ofolder->name == NULL || SDL_strcmp(name, ofolder->name) != 0)
                    continue;

                append = TRUE;
                append_idx = i;
                folder = *ofolder;
                break;
            }
        }
    }

    yyjson_val* jarr = jval;
    if (yyjson_is_obj(jval)) {
        if (folder.name == NULL) {
            const char* name = yyjson_get_str(yyjson_obj_get(jval, "name"));
            if (name != NULL) {
                folder.name = SDL_strdup(name);
                if (folder.name == NULL)
                    WARN("Failed to allocate editor folder \"%s\" name", name);
            }
        }

        jarr = yyjson_obj_get(jval, "items");
    }

    for (size_t i = 0, n = yyjson_arr_size(jarr); i < n; i++) {
        yyjson_val* jval2 = yyjson_arr_get(jarr, i);
        if (yyjson_is_obj(jval2)) {
            create_folder(&folder, jval2);
        } else if (yyjson_is_str(jval2)) {
            const char* def = yyjson_get_str(jval2);
            if (folder.defs == NULL)
                folder.defs = MakeTinyDPro(1, sizeof(*folder.defs));
            folder.defs = TinyDPush(folder.defs, &(TinyHash){StHashStr(def)});
        }
    }

    if (parent == NULL) {
        if (editor->folders == NULL) {
            editor->folders = SDL_malloc(sizeof(folder));
            EXPECT(editor->folders, "Failed to allocate editor folder \"%s\"", folder.name);
        }
        *editor->folders = folder;
    } else if (append && parent->folders != NULL) {
        parent->folders[append_idx] = folder;
    } else {
        if (parent->folders == NULL)
            parent->folders = MakeTinyDPro(1, sizeof(*parent->folders));
        parent->folders = TinyDPush(parent->folders, &folder);
    }
}
// @NOLINTEND(misc-no-recursion)

typedef struct {
    TinyHash key;
    EditorDef* def;
} LastDefData;

static void iterate_editor_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    (void)filename;
    (void)userdata;

    yyjson_doc* json = read_json(buffer, size, NULL);
    if (json == NULL)
        return;

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        WTF("Expected editor.json root as object, got %s", yyjson_get_type_desc(root));
        yyjson_doc_free(json);
        return;
    }

    yyjson_val* jval = yyjson_obj_get(root, "defs");
    LastDefData* last = userdata;
    size_t i = 0, n = 0;
    yyjson_val *jkey = NULL, *jdef = NULL;
    yyjson_obj_foreach(jval, i, n, jkey, jdef) {
        if (!yyjson_is_obj(jdef))
            continue;

        const char* name = yyjson_get_str(jkey);
        if (name == NULL) {
            WTF("Def %zu has no name", i);
            continue;
        }

        const TinyHash key = StHashStr(name);
        EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, key);
        if (def == NULL) {
            TinyBucket* bucket = TinyMapPut(&editor->defs, key, &(EditorDef){0}, sizeof(EditorDef));
            bucket->cleanup = nuke_def;
            def = bucket->data;

            def->max_scale[0] = def->max_scale[1] = 1000000.f;
            for (size_t i = 0; i < SDL_arraysize(def->colors); i++)
                for (size_t j = 0; j < SDL_arraysize(*def->colors); j++)
                    def->colors[i][j] = 1.f;

            def->previous = last->key;
        }

        def->name = SDL_strdup(name);
        EXPECT(def->name, "Failed to allocate def \"%s\" name", name);

        const char* sprite = yyjson_get_str(yyjson_obj_get(jdef, "sprite"));
        if (sprite != NULL) {
            def->sprite = SDL_strdup(sprite);
            EXPECT(def->sprite, "Failed to allocate def \"%s\" sprite \"%s\"", name, sprite);
            load_sprite(sprite, AKL_NEVER);
        }

        const char* type = yyjson_get_str(yyjson_obj_get(jdef, "type"));
        if (type != NULL) {
            if (SDL_strcmp(type, "background") == 0)
                def->type = DEFT_BACKGROUND;
            else if (SDL_strcmp(type, "tile") == 0)
                def->type = DEFT_TILE;
            else if (SDL_strcmp(type, "actor") == 0)
                def->type = DEFT_ACTOR;
            else
                WARN("Def \"%s\" has invalid type \"%s\"", name, type);
        }

        if (def->type == DEFT_ACTOR) {
            def->actor = yyjson_get_uint(yyjson_obj_get(jdef, "id"));
            if (def->actor == ACT_NULL)
                WARN("Actor def \"%s\" has null type", name);
        }

        yyjson_val* jdval = yyjson_obj_get(jdef, "flip");
        def->flip[0] = yyjson_get_bool(yyjson_arr_get(jdval, 0));
        def->flip[1] = yyjson_get_bool(yyjson_arr_get(jdval, 1));

        jdval = yyjson_obj_get(jdef, "tile");
        def->tile[0] = yyjson_get_bool(yyjson_arr_get(jdval, 0));
        def->tile[1] = yyjson_get_bool(yyjson_arr_get(jdval, 1));

        jdval = yyjson_obj_get(jdef, "scalable");
        if (yyjson_is_bool(jdval)) {
            def->scalable = yyjson_get_bool(jdval);
            if (def->scalable && def->type == DEFT_ACTOR) {
                const Sprite* spr = get_sprite(def->sprite);
                if (spr != NULL) {
                    def->max_scale[0] = 256.f / spr->size[0];
                    def->max_scale[1] = 256.f / spr->size[1];
                }
            }
        } else {
            jdval = yyjson_obj_get(jdef, "max_scale");
            if (yyjson_is_arr(jdval) && yyjson_arr_size(jdval) >= 2) {
                def->scalable = TRUE;
                def->max_scale[0] = (float)yyjson_get_num(yyjson_arr_get(jdval, 0));
                def->max_scale[1] = (float)yyjson_get_num(yyjson_arr_get(jdval, 1));
            }
        }

        def->depth = (int)yyjson_get_sint(yyjson_obj_get(jdef, "depth"));

        jdval = yyjson_obj_get(jdef, "colors");
        for (size_t j = 0, n2 = yyjson_arr_size(jdval); j < n2 && j < SDL_arraysize(def->colors); j++) {
            yyjson_val* jcolor = yyjson_arr_get(jdval, j);
            for (size_t k = 0, n3 = yyjson_arr_size(jcolor); k < n3 && k < SDL_arraysize(*def->colors); k++)
                def->colors[j][k] = (float)yyjson_get_uint(yyjson_arr_get(jcolor, k)) / 255.f;
        }

        if (def->type == DEFT_ACTOR) {
            jdval = yyjson_obj_get(jdef, "values");
            for (size_t j = 0, n2 = yyjson_arr_size(jdval); j < n2; j++) {
                yyjson_val* jvalue = yyjson_arr_get(jdval, j);
                if (!yyjson_is_obj(jvalue))
                    continue;

                EditorDefValue value = {0};

                const char* vname = yyjson_get_str(yyjson_obj_get(jvalue, "name"));
                if (vname != NULL)
                    value.name = SDL_strdup(vname);

                value.index = yyjson_get_uint(yyjson_obj_get(jvalue, "index")) % MAX_VALUES;
                value.default_value = yyjson_get_int(yyjson_obj_get(jvalue, "default"));
                value.hidden = yyjson_get_bool(yyjson_obj_get(jvalue, "hidden"));

                if (def->values == NULL)
                    def->values = MakeTinyDPro(1, sizeof(value));
                def->values = TinyDPush(def->values, &value);
            }

            jdval = yyjson_obj_get(jdef, "flags");
            for (size_t j = 0, n2 = yyjson_arr_size(jdval); j < n2; j++) {
                yyjson_val* jflag = yyjson_arr_get(jdval, j);
                if (!yyjson_is_obj(jflag))
                    continue;

                EditorDefFlag flag = {0};

                const char* fname = yyjson_get_str(yyjson_obj_get(jflag, "name"));
                if (fname != NULL)
                    flag.name = SDL_strdup(fname);

                flag.index = yyjson_get_uint(yyjson_obj_get(jflag, "index")) % (8 * sizeof(ActorFlags));
                flag.default_value = yyjson_get_bool(yyjson_obj_get(jflag, "default"));
                flag.hidden = yyjson_get_bool(yyjson_obj_get(jflag, "hidden"));

                if (def->flags == NULL)
                    def->flags = MakeTinyDPro(1, sizeof(flag));
                def->flags = TinyDPush(def->flags, &flag);
            }
        }

        if (last->def != NULL)
            last->def->next = key;
        last->def = def;
        last->key = key;
    }

    jval = yyjson_obj_get(root, "items");
    if (yyjson_is_arr(jval))
        create_folder(NULL, jval);

    yyjson_doc_free(json);
}

static void start(const void* secret, size_t secret_size) {
    (void)secret;
    (void)secret_size;

    CIMGUI_CHECKVERSION();

    ImGui_CreateContext(NULL);
    extern SDL_GLContext GPU;
    cImGui_ImplSDL3_InitForOpenGL(WINDOW, GPU);
    cImGui_ImplOpenGL3_Init();

    editor = SDL_calloc(1, sizeof(*editor));
    EXPECT(editor, "Failed to allocate editor state");

    clear_level();
    editor->cursor.grid_size = 32;
    editor->camera.show_grid = TRUE;
    editor->camera.zoom = 1.f;

    iterate_data_files("editor.json", TRUE, iterate_editor_file, &(LastDefData){0});
}

// @NOLINTBEGIN(misc-no-recursion)
static void destroy_folder(EditorFolder* folder) {
    SDL_free((void*)folder->name);

    for (size_t i = 0, n = TinyDLength(folder->folders); i < n; i++)
        destroy_folder(&folder->folders[i]);
    FreeTinyD(folder->folders);

    FreeTinyD(folder->defs);

    if (folder == editor->folders)
        SDL_free(folder);
}
// @NOLINTEND(misc-no-recursion)

static void end() {
    cImGui_ImplOpenGL3_Shutdown();
    cImGui_ImplSDL3_Shutdown();
    ImGui_DestroyContext(NULL);

    for (AsyncID i = 0; i < (AsyncID)ASYNC_SIZE; i++)
        clear_async(i);
    clear_level();
    destroy_surface(editor->blueprint);
    FreeTinyMap(&editor->defs);
    destroy_folder(editor->folders);
    SDL_free(editor);

    rediscover_mods();
}

static void event(const SDL_Event* event) {
    cImGui_ImplSDL3_ProcessEvent(event);
}

static void draw_highlight(const EditorMarker* marker) {
    float bx = (float)marker->pos[0], by = (float)marker->pos[1];
    float bw = marker->scale[0], bh = marker->scale[1];

    const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
    if (def != NULL && !(marker->tile[0] || marker->tile[1])) {
        const Sprite* sprite = get_sprite(def->sprite);
        if (sprite != NULL) {
            bx -= sprite->offset[0] * bw;
            by -= sprite->offset[1] * bh;
            bw *= sprite->size[0];
            bh *= sprite->size[1];
        }
    }

    batch_blend(BM_ADD);
    batch_pos(B_F3_XY(bx, by));
    batch_color(B_U4_VALUE(64));
    batch_rectangle(NULL, B_F2(bw, bh));
    batch_blend(BM_NORMAL);
}

static void draw() {
    // GROSS HACK: drawing is done in main thread, so poll async events here.
    if (has_async(ASYNC_OPEN)) {
        open_level(get_async(ASYNC_OPEN));
        clear_async(ASYNC_OPEN);
    }

    if (has_async(ASYNC_BLUEPRINT)) {
        destroy_surface(editor->blueprint);
        editor->blueprint = create_surface_from_file(get_async(ASYNC_BLUEPRINT));
        clear_async(ASYNC_BLUEPRINT);
    }

    // Actual drawing
    clear_color(B_F4_VALUE(0.5f));

    int width = 0, height = 0;
    get_resolution(&width, &height);

    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    const EditorCamera* ecamera = &editor->camera;
    glm_ortho(ecamera->pos[0], ecamera->pos[0] + (ecamera->zoom * (float)width),
        ecamera->pos[1] + (ecamera->zoom * (float)height), ecamera->pos[1], -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_filter(FALSE);

    batch_reset();
    batch_color(B_U4_ALPHA(128));
    batch_surface(editor->blueprint);

    TinyPq sorter = {0};

    const EditorLevel* elevel = &editor->level;
    for (size_t i = 0, n = TinyDLength(elevel->markers); i < n; i++) {
        const EditorMarker* marker = &elevel->markers[i];
        TinyPqInsert(&sorter, marker->pos[2], (void*)&marker, sizeof(EditorMarker*));
    }

    TINY_PQ_FOREACH (&sorter, iter) {
        const EditorMarker* marker = *(EditorMarker**)iter.data;

        batch_pos(B_F3(marker->pos[0], marker->pos[1], marker->pos[2]));
        batch_flip(
            B_B2(marker->flip[0] || (marker->flags & FLG_X_FLIP), marker->flip[1] || (marker->flags & FLG_Y_FLIP)));
        batch_tile(marker->tile);
        batch_colors(B_U4X4_F4X4(marker->colors));

        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
        const Sprite* sprite = get_sprite(def->sprite);
        if (sprite == NULL) {
            batch_scale(B_F2_1);
            batch_rectangle(NULL, marker->scale);
        } else if (marker->tile[0] || marker->tile[1]) {
            const Texture* texture = get_texture_key(sprite->texture_key);
            batch_scale(B_F2_1);
            batch_rectangle((texture == NULL) ? NULL : texture->base.name, marker->scale);
        } else {
            batch_scale(marker->scale);
            batch_sprite(def->sprite);
        }
    }

    FreeTinyPq(&sorter);

    batch_scale(B_F2_1);
    batch_flip(B_B2_FALSE);
    batch_tile(B_B2_FALSE);

    const EditorCursor* ecursor = &editor->cursor;
    if (ecamera->show_grid && ecursor->grid_size >= 2) {
        const float gsize = ecursor->grid_size;
        if ((gsize * ecamera->zoom) >= 2.f) {
            const float cw = (float)width * ecamera->zoom, ch = (float)height * ecamera->zoom;
            const float cx1 = ecamera->pos[0], cy1 = ecamera->pos[1], cx2 = cx1 + cw, cy2 = cy1 + ch;

            batch_color(B_U4(0, 0, 0, 64));
            for (float i = SDL_floorf(cx1 / gsize) * gsize; i <= cx2; i += gsize) {
                batch_pos(B_F3_XY(i, cy1 - gsize));
                batch_rectangle(NULL, B_F2(ecamera->zoom, ch + gsize));
            }
            for (float i = SDL_floorf(cy1 / gsize) * gsize; i <= cy2; i += gsize) {
                batch_pos(B_F3_XY(cx1 - gsize, i));
                batch_rectangle(NULL, B_F2(cw + gsize, ecamera->zoom));
            }
        }
    }

    batch_pos(B_F3_XY(0.f, -32.f));
    batch_color(B_U4_ALPHA(128));
    batch_rectangle(NULL, B_F2(ecamera->zoom, 64.f));
    batch_pos(B_F3_XY(-32.f, 0.f));
    batch_rectangle(NULL, B_F2(64.f, ecamera->zoom));

    batch_pos(B_F3_0);
    batch_rectangle(NULL, B_F2(elevel->size[0], ecamera->zoom));
    batch_rectangle(NULL, B_F2(ecamera->zoom, elevel->size[1]));
    batch_pos(B_F3_XY(elevel->size[0] - ecamera->zoom, 0.f));
    batch_rectangle(NULL, B_F2(ecamera->zoom, elevel->size[1]));
    batch_pos(B_F3_XY(0.f, elevel->size[1] - ecamera->zoom));
    batch_rectangle(NULL, B_F2(elevel->size[0], ecamera->zoom));

    batch_pos(B_F3_XY(elevel->bounds[0], elevel->bounds[1]));
    const int bw = elevel->bounds[2] - elevel->bounds[0], bh = elevel->bounds[3] - elevel->bounds[1];
    batch_rectangle(NULL, B_F2(bw, ecamera->zoom));
    batch_rectangle(NULL, B_F2(ecamera->zoom, bh));
    batch_pos(B_F3_XY(elevel->bounds[2] - ecamera->zoom, 0.f));
    batch_rectangle(NULL, B_F2(ecamera->zoom, bh));
    batch_pos(B_F3_XY(0.f, elevel->bounds[3] - ecamera->zoom));
    batch_rectangle(NULL, B_F2(bw, ecamera->zoom));

    if (ecursor->has_highlighted)
        draw_highlight(&elevel->markers[ecursor->highlighted]);
    if (ecursor->has_selected)
        draw_highlight(&elevel->markers[ecursor->selected]);

    const EditorDef* cdef = (EditorDef*)TinyMapGet(&editor->defs, editor->def_key);
    if (cdef != NULL) {
        const Sprite* sprite = get_sprite(cdef->sprite);
        if (!ecursor->has_scalable && !ecursor->has_highlighted) {
            batch_pos(B_F3_XY(ecursor->pos[0], ecursor->pos[1]));

            float colors[4][4] = {0};
            SDL_memcpy(colors, cdef->colors, sizeof(colors));
            colors[0][3] *= 0.5f;
            colors[1][3] *= 0.5f;
            colors[2][3] *= 0.5f;
            colors[3][3] *= 0.5f;
            batch_colors(B_U4X4_F4X4(colors));

            if (sprite == NULL)
                batch_rectangle(NULL, B_F2_S(32.f));
            else
                batch_sprite(sprite->base.name);
        }

        glm_ortho(0.f, (float)width, (float)height, 0.f, -16000.f, 16000.f, proj);
        set_projection_matrix(proj);
        apply_matrices();

        batch_colors(B_U4X4_F4X4(cdef->colors));

        if (sprite == NULL) {
            batch_pos(B_F3_XY(width - 80.f, height - 80.f));
            batch_rectangle(NULL, B_F2_S(64.f));
        } else {
            const float scale = 64.f / sprite->size[1];
            batch_pos(B_F3_XY(width - 16.f - ((sprite->size[0] - sprite->offset[0]) * scale),
                height - 16.f - ((sprite->size[1] - sprite->offset[1]) * scale)));
            batch_scale(B_F2_S(scale));
            batch_sprite(sprite->base.name);
        }
    }

    batch_filter(TRUE);
}

// @NOLINTBEGIN(misc-no-recursion)
static void show_folder(EditorFolder* folder) {
    if (folder == NULL)
        return;

    for (size_t i = 0, n = TinyDLength(folder->folders); i < n; i++) {
        EditorFolder* fold = &folder->folders[i];
        if (ImGui_BeginMenu(fold->name)) {
            show_folder(fold);
            ImGui_EndMenu();
        }
    }

    for (size_t i = 0, n = TinyDLength((void*)folder->defs); i < n; i++) {
        const TinyHash key = folder->defs[i];
        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, key);
        if (def != NULL && ImGui_MenuItem(def->name))
            editor->def_key = key;
    }
}
// @NOLINTEND(misc-no-recursion)

static void draw_ui() {
    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplOpenGL3_NewFrame();
    ImGui_NewFrame();

    EditorCamera* ecamera = &editor->camera;

    const ImGuiIO* io = ImGui_GetIO();
    if (!io->WantCaptureKeyboard) {
        if (ImGui_IsKeyPressed(ImGuiKey_F1))
            open_blueprint_dialog();

        if (ImGui_IsKeyPressed(ImGuiKey_G))
            ecamera->show_grid = !ecamera->show_grid;

        if (ImGui_IsKeyPressed(ImGuiKey_R)) {
            ecamera->pos[0] = ecamera->pos[1] = ecamera->hold[0] = ecamera->hold[1] = 0.f;
            ecamera->zoom = 1.f;
        }

        const EditorDef* cdef = (EditorDef*)TinyMapGet(&editor->defs, editor->def_key);
        if (cdef != NULL) {
            if (ImGui_IsKeyPressed(ImGuiKey_Q))
                editor->def_key = cdef->previous;
            if (ImGui_IsKeyPressed(ImGuiKey_E))
                editor->def_key = cdef->next;
        }
    }

    EditorCursor* ecursor = &editor->cursor;
    if (!io->WantCaptureMouse) {
        ImVec2 mpos = ImGui_GetMousePos();

        if (ImGui_IsMouseClicked(ImGuiMouseButton_Middle)) {
            ecamera->hold[0] = ecamera->pos[0] + (mpos.x * ecamera->zoom);
            ecamera->hold[1] = ecamera->pos[1] + (mpos.y * ecamera->zoom);
        }
        if (ImGui_IsMouseDown(ImGuiMouseButton_Middle)) {
            const float tx = ecamera->pos[0] + (mpos.x * ecamera->zoom),
                        ty = ecamera->pos[1] + (mpos.y * ecamera->zoom);
            ecamera->pos[0] += ecamera->hold[0] - tx;
            ecamera->pos[1] += ecamera->hold[1] - ty;
        }

        const float wheel = io->MouseWheel;
        if (wheel != 0.f) {
            const float omx = ecamera->pos[0] + (mpos.x * ecamera->zoom),
                        omy = ecamera->pos[1] + (mpos.y * ecamera->zoom);

            ecamera->zoom -= wheel / ((ecamera->zoom > 1.f || (ecamera->zoom == 1.f && wheel < 0.f)) ? 4.f : 10.f);
            ecamera->zoom = SDL_clamp(ecamera->zoom, 0.1f, 5.f);

            ecamera->pos[0] -= (ecamera->pos[0] + (mpos.x * ecamera->zoom)) - omx;
            ecamera->pos[1] -= (ecamera->pos[1] + (mpos.y * ecamera->zoom)) - omy;
        }

        move_cursor(
            (Sint32[2]){
                (Sint32)ecamera->pos[0] + (Sint32)(mpos.x * ecamera->zoom),
                (Sint32)ecamera->pos[1] + (Sint32)(mpos.y * ecamera->zoom),
            },
            !ImGui_IsKeyDown(ImGuiKey_LeftShift), !ImGui_IsKeyDown(ImGuiKey_LeftAlt));

        if (ecursor->has_scalable) {
            EditorMarker* marker = &editor->level.markers[ecursor->scalable];

            const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
            const Sprite* sprite = (def == NULL || marker->tile[0] || marker->tile[1]) ? NULL : get_sprite(def->sprite);

            float sprite_width = 1.f, sprite_height = 1.f;
            if (sprite != NULL) {
                sprite_width = sprite->size[0];
                sprite_height = sprite->size[1];
            }

            float max_width = 1000000.f, max_height = 1000000.f;
            if (def != NULL) {
                max_width = def->max_scale[0] * sprite_width;
                max_height = def->max_scale[1] * sprite_height;
            }

            float sx = (float)ecursor->pos[0] - (float)marker->pos[0],
                  sy = (float)ecursor->pos[1] - (float)marker->pos[1];
            marker->scale[0] = SDL_clamp(sx, 16.f, max_width) / sprite_width;
            marker->scale[1] = SDL_clamp(sy, 16.f, max_height) / sprite_height;

            if (!ImGui_IsMouseDown(ImGuiMouseButton_Left))
                ecursor->has_scalable = FALSE;
        } else {
            if (ImGui_IsMouseClicked(ImGuiMouseButton_Left)
                || (ImGui_IsKeyDown(ImGuiKey_LeftCtrl) && ImGui_IsMouseDown(ImGuiMouseButton_Left)
                    && !ecursor->has_highlighted))
            {

                if (ecursor->has_highlighted) {
                    ecursor->has_selected = TRUE;
                    ecursor->selected = ecursor->highlighted;
                    ecursor->has_highlighted = FALSE;
                } else {
                    const EditorDef* cdef = (EditorDef*)TinyMapGet(&editor->defs, editor->def_key);
                    if (cdef != NULL) {
                        EditorMarker marker = init_marker();

                        marker.def_key = editor->def_key;
                        marker.pos[0] = ecursor->pos[0];
                        marker.pos[1] = ecursor->pos[1];
                        marker.pos[2] = cdef->depth;
                        marker.flip[0] = cdef->flip[0];
                        marker.flip[1] = cdef->flip[1];
                        marker.tile[0] = cdef->tile[0];
                        marker.tile[1] = cdef->tile[1];
                        SDL_memcpy(marker.colors, cdef->colors, sizeof(marker.colors));

                        for (size_t i = 0, n = TinyDLength(cdef->values); i < n; i++) {
                            const EditorDefValue* dvalue = &cdef->values[i];
                            marker.values[dvalue->index] = dvalue->default_value;
                        }

                        for (size_t i = 0, n = TinyDLength(cdef->flags); i < n; i++) {
                            const EditorDefFlag* dflag = &cdef->flags[i];
                            marker.flags |= dflag->default_value << dflag->index;
                        }

                        if (editor->level.markers == NULL)
                            editor->level.markers = MakeTinyDPro(1, sizeof(*editor->level.markers));
                        editor->level.markers = TinyDPush(editor->level.markers, &marker);

                        if (cdef->scalable) {
                            ecursor->has_scalable = TRUE;
                            ecursor->scalable = TinyDLength(editor->level.markers) - 1;
                        } else {
                            ecursor->has_highlighted = TRUE;
                            ecursor->highlighted = TinyDLength(editor->level.markers) - 1;
                        }
                    }
                }
            }

            if (ImGui_IsMouseClicked(ImGuiMouseButton_Middle) && ecursor->has_highlighted)
                editor->def_key = editor->level.markers[ecursor->highlighted].def_key;

            if ((ImGui_IsMouseClicked(ImGuiMouseButton_Right)
                    || (ImGui_IsKeyDown(ImGuiKey_LeftCtrl) && ImGui_IsMouseDown(ImGuiMouseButton_Right)))
                && ecursor->has_highlighted)
            {
                editor->level.markers = TinyDErase(editor->level.markers, ecursor->highlighted);
                if (ecursor->has_selected && ecursor->selected == ecursor->highlighted)
                    ecursor->has_selected = FALSE;
                ecursor->has_highlighted = FALSE;
            }
        }
    }

    if (ImGui_BeginMainMenuBar()) {
        if (ImGui_BeginMenu(LFMT("editor.file"))) {
            if (ImGui_MenuItem(LFMT("editor.new")))
                clear_level();

            if (ImGui_MenuItem(LFMT("editor.open")))
                open_level_dialog();

            if (ImGui_MenuItem(LFMT("editor.save_as")))
                save_level_dialog();

            ImGui_Separator();

            if (ImGui_MenuItem(LFMT("editor.exit")))
                set_screen(SCR_MENU, NULL, 0);

            ImGui_EndMenu();
        }

        if (ImGui_BeginMenu(LFMT("editor.level"))) {
            EditorLevel* elevel = &editor->level;

            if (ImGui_CollapsingHeader(LFMT("editor.strings"), 0)) {
                ImGui_InputText(LFMT("editor.label"), elevel->label, sizeof(elevel->label), 0);

                if (ImGui_TreeNode(LFMT("editor.tracks"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->tracks); i++) {
                        ImGui_InputText(
                            LFMT("editor.track", 'd', i + 1), elevel->tracks[i], sizeof(elevel->tracks[i]), 0);
                    }

                    ImGui_TreePop();
                    ImGui_Spacing();
                }

                if (ImGui_TreeNode(LFMT("editor.warps"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->warps); i++)
                        ImGui_InputText(LFMT("editor.warp", 'd', i + 1), elevel->warps[i], sizeof(elevel->warps[i]), 0);

                    ImGui_TreePop();
                    ImGui_Spacing();
                }

                if (ImGui_TreeNode(LFMT("editor.secrets"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->secrets); i++) {
                        ImGui_InputText(
                            LFMT("editor.secret", 'd', i + 1), elevel->secrets[i], sizeof(elevel->secrets[i]), 0);
                    }

                    ImGui_TreePop();
                    ImGui_Spacing();
                }
            }

            if (ImGui_CollapsingHeader(LFMT("editor.constants"), 0)) {}

            if (ImGui_CollapsingHeader(LFMT("editor.flags"), 0)) {
                ImGui_CheckboxFlagsUintPtr(LFMT("editor.hardcore"), &elevel->flags, GF_HARDCORE);
                ImGui_CheckboxFlagsUintPtr(LFMT("editor.lost_map"), &elevel->flags, GF_LOST_MAP);
                ImGui_CheckboxFlagsUintPtr(LFMT("editor.funny_tanks"), &elevel->flags, GF_FUNNY_TANKS);
                ImGui_Checkbox(LFMT("editor.ambush"), (bool*)&elevel->ambush);
            }

            ImGui_Separator();

            ImGui_InputInt2(LFMT("editor.size"), elevel->size, 0);
            ImGui_InputInt4(LFMT("editor.bounds"), elevel->bounds, 0);
            ImGui_Separator();
            ImGui_InputInt(LFMT("editor.time"), &elevel->time);

            ImGui_EndMenu();
        }

        if (ImGui_BeginMenu(LFMT("editor.markers"))) {
            show_folder(editor->folders);
            ImGui_EndMenu();
        }

        ImGui_Separator();
        const EditorCursor* ecursor = &editor->cursor;
        ImGui_Text("X: %i Y: %i", ecursor->pos[0], ecursor->pos[1]);
        ImGui_Separator();
        ImGui_Text("%s: %.0f%%", LFMT("editor.zoom"), 100.f / ecamera->zoom);
        ImGui_Separator();
        ImGui_Text("%s: %s, %ux%u", LFMT("editor.grid"), LFMT(ecamera->show_grid ? "editor.on" : "editor.off"),
            ecursor->grid_size, ecursor->grid_size);
        ImGui_Separator();
        ImGui_Text("%s: %zu", LFMT("editor.markers"), TinyDLength(editor->level.markers));
        ImGui_Separator();

        ImGui_EndMainMenuBar();
    }

    if (ecursor->has_selected) {
        EditorMarker* marker = &editor->level.markers[ecursor->selected];
        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, marker->def_key);
        if (def != NULL) {
            if (ImGui_Begin(def->name, (bool*)&ecursor->has_selected, 0)) {
                ImGui_InputInt2(LFMT("editor.position"), marker->pos, 0);
                ImGui_InputInt(LFMT("editor.depth"), &marker->pos[2]);
                ImGui_InputFloat2(
                    LFMT((marker->tile[0] || marker->tile[1] || get_sprite(def->sprite) == NULL) ? "editor.size"
                                                                                                 : "editor.scale"),
                    marker->scale);
                ImGui_Spacing();
                if (def->type == DEFT_ACTOR) {
                    ImGui_InputFloat2(LFMT("editor.velocity"), marker->vel);
                    ImGui_Checkbox(LFMT("editor.spawn_once"), (bool*)&marker->spawn_once);
                    ImGui_Checkbox(LFMT("editor.spawn_singleplayer"), (bool*)&marker->spawn_singleplayer);
                    ImGui_SameLine();
                    ImGui_Checkbox(LFMT("editor.spawn_multiplayer"), (bool*)&marker->spawn_multiplayer);
                    ImGui_Separator();

                    for (size_t i = 0, n = TinyDLength(def->values); i < n; i++) {
                        const EditorDefValue* dvalue = &def->values[i];
                        if (!dvalue->hidden)
                            ImGui_InputInt(fmt("%s##value", dvalue->name), &marker->values[dvalue->index]);
                    }

                    for (size_t i = 0, n = TinyDLength(def->flags); i < n; i++) {
                        const EditorDefFlag* dflag = &def->flags[i];
                        if (!dflag->hidden)
                            ImGui_CheckboxFlagsUintPtr(fmt("%s##flag", dflag->name), &marker->flags, 1 << dflag->index);
                    }
                } else {
                    ImGui_Checkbox(LFMT("editor.flip_x"), (bool*)&marker->flip[0]);
                    ImGui_SameLine();
                    ImGui_Checkbox(LFMT("editor.flip_y"), (bool*)&marker->flip[1]);
                    ImGui_Checkbox(LFMT("editor.tile_x"), (bool*)&marker->tile[0]);
                    ImGui_SameLine();
                    ImGui_Checkbox(LFMT("editor.tile_y"), (bool*)&marker->tile[1]);
                    ImGui_Spacing();
                    ImGui_ColorEdit4(LFMT("editor.top_left"), marker->colors[0], ImGuiColorEditFlags_Uint8);
                    ImGui_ColorEdit4(LFMT("editor.top_right"), marker->colors[1], ImGuiColorEditFlags_Uint8);
                    ImGui_ColorEdit4(LFMT("editor.bottom_left"), marker->colors[2], ImGuiColorEditFlags_Uint8);
                    ImGui_ColorEdit4(LFMT("editor.bottom_right"), marker->colors[3], ImGuiColorEditFlags_Uint8);
                }
            }

            ImGui_End();
        }
    }

    if (editor->error != NULL)
        ImGui_OpenPopup(LFMT("editor.error"), 0);
    if (ImGui_BeginPopupModal(LFMT("editor.error"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui_Text("%s", LFMT(editor->error));
        ImGui_Spacing();

        if (ImGui_Button(LFMT("editor.ok"))) {
            editor->error = NULL;
            ImGui_CloseCurrentPopup();
        }

        ImGui_EndPopup();
    }

    ImGui_Render();
    cImGui_ImplOpenGL3_RenderDrawData(ImGui_GetDrawData());
}

const ScreenTable TAB_EDITOR = {
    .start = start,
    .end = end,
    .event = event,
    .draw = draw,
    .draw_ui = draw_ui,
};

//
//
//
#else
//
//
//

const ScreenTable TAB_EDITOR = {};

//
//
//
#endif // SDL_PLATFORM_EMSCRIPTEN
