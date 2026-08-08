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

typedef struct {
    Bool available;
    const char* ptr;
} EditorAsync;

typedef struct {
    Uint8 colors[4][4];
    TinyHash previous, next;
    const char *name, *sprite;
} EditorDef;

typedef struct EditorFolder {
    const char* name;
    struct EditorFolder* folders;
    TinyHash* defs;
} EditorFolder;

typedef struct {
    Uint16 grid_size;
    Sint32 pos[2];
    void* highlighted;
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
} EditorLevel;

typedef struct {
    EditorCursor cursor;
    EditorCamera camera;
    EditorLevel level;

    EditorAsync async[ASYNC_SIZE];
    const char* error;
    Surface* blueprint;

    TinyMap defs;
    const EditorDef* current_def;

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

static void move_cursor(const Sint32 pos[2], Bool snap) {
    if (ImGui_GetIO()->WantCaptureMouse || (pos[0] == SDL_MIN_SINT32 || pos[1] == SDL_MIN_SINT32))
        return;

    EditorCursor* ecursor = &editor->cursor;
    Sint32 ox = ecursor->pos[0], oy = ecursor->pos[1];
    if (pos[0] != ox || pos[1] != oy) {
        // TODO: Highlight markers
        ecursor->highlighted = NULL;
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
    // Cleanup

    // Nullify
    EditorLevel* level = &editor->level;
    SDL_zerop(level);
    level->size[0] = SCREEN_WIDTH;
    level->size[1] = SCREEN_HEIGHT;
    level->bounds[2] = SCREEN_WIDTH;
    level->bounds[3] = SCREEN_HEIGHT;
    level->time = -1;
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
    elevel->size[0] = (int)yyjson_get_uint(yyjson_arr_get(jval, 0));
    elevel->size[1] = (int)yyjson_get_uint(yyjson_arr_get(jval, 1));

    jval = yyjson_obj_get(root, "bounds");
    elevel->bounds[0] = (int)yyjson_get_sint(yyjson_arr_get(jval, 0));
    elevel->bounds[1] = (int)yyjson_get_sint(yyjson_arr_get(jval, 1));
    elevel->bounds[2] = (int)yyjson_get_sint(yyjson_arr_get(jval, 2));
    elevel->bounds[3] = (int)yyjson_get_sint(yyjson_arr_get(jval, 3));

    elevel->time = (int)yyjson_get_sint(yyjson_obj_get(root, "time"));

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

    if (elevel->size[0] != 0 || elevel->size[1] != 0) {
        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "size");
        yyjson_mut_arr_add_uint(json, jval, elevel->size[0]);
        yyjson_mut_arr_add_uint(json, jval, elevel->size[1]);
    }

    if (elevel->bounds[0] != 0 || elevel->bounds[1] != 0 || elevel->bounds[2] != 0 || elevel->bounds[3] != 0) {
        yyjson_mut_val* jval = yyjson_mut_obj_add_arr(json, root, "bounds");
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[0]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[1]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[2]);
        yyjson_mut_arr_add_uint(json, jval, elevel->bounds[3]);
    }

    if (elevel->time != 0)
        yyjson_mut_obj_add_sint(json, root, "time", elevel->time);

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
    EditorDef* last_def = NULL;
    TinyHash last_key = 0;
    size_t i = 0, n = 0;
    yyjson_val *jkey = NULL, *jval2 = NULL;
    yyjson_obj_foreach(jval, i, n, jkey, jval2) {
        if (!yyjson_is_obj(jval2))
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

            SDL_memset(def->colors, 255, sizeof(def->colors));
            def->previous = last_key;
        }

        def->name = SDL_strdup(name);
        EXPECT(def->name, "Failed to allocate def \"%s\" name", name);

        const char* sprite = yyjson_get_str(yyjson_obj_get(jval2, "sprite"));
        if (sprite != NULL) {
            def->sprite = SDL_strdup(sprite);
            EXPECT(def->sprite, "Failed to allocate def \"%s\" sprite \"%s\"", name, sprite);
            load_sprite(sprite, AKL_NEVER);
        }

        yyjson_val* jval3 = yyjson_obj_get(jval2, "colors");
        for (size_t j = 0, n2 = yyjson_arr_size(jval3); j < n2 && j < 4; j++) {
            yyjson_val* jval4 = yyjson_arr_get(jval3, j);
            for (size_t k = 0, n3 = yyjson_arr_size(jval4); k < n3 && k < 4; k++)
                def->colors[j][k] = yyjson_get_uint(yyjson_arr_get(jval4, k));
        }

        if (last_def != NULL)
            last_def->next = key;
        last_def = def;
        last_key = key;
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

    iterate_data_files("editor.json", TRUE, iterate_editor_file, NULL);
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
    // TODO: Draw markers

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

    const EditorLevel* elevel = &editor->level;
    batch_pos(B_F3_0);
    batch_rectangle(NULL, B_F2(elevel->size[0], ecamera->zoom));
    batch_rectangle(NULL, B_F2(ecamera->zoom, elevel->size[1]));
    batch_pos(B_F3_XY(elevel->size[0], 0.f));
    batch_rectangle(NULL, B_F2(ecamera->zoom, elevel->size[1]));
    batch_pos(B_F3_XY(0.f, elevel->size[1]));
    batch_rectangle(NULL, B_F2(elevel->size[0], ecamera->zoom));

    batch_pos(B_F3_XY(elevel->bounds[0], elevel->bounds[1]));
    const int bw = elevel->bounds[2] - elevel->bounds[0], bh = elevel->bounds[3] - elevel->bounds[1];
    batch_rectangle(NULL, B_F2(bw, ecamera->zoom));
    batch_rectangle(NULL, B_F2(ecamera->zoom, bh));
    batch_pos(B_F3_XY(elevel->bounds[2], 0.f));
    batch_rectangle(NULL, B_F2(ecamera->zoom, bh));
    batch_pos(B_F3_XY(0.f, elevel->bounds[3]));
    batch_rectangle(NULL, B_F2(bw, ecamera->zoom));

    if (editor->current_def == NULL)
        return;

    batch_pos(B_F3_XY(ecursor->pos[0], ecursor->pos[1]));

    Uint8 colors[4][4] = {0};
    SDL_memcpy(colors, editor->current_def->colors, sizeof(colors));
    colors[0][3] /= 2;
    colors[1][3] /= 2;
    colors[2][3] /= 2;
    colors[3][3] /= 2;
    batch_colors(colors);

    const Sprite* sprite = get_sprite(editor->current_def->sprite);
    if (sprite == NULL)
        batch_rectangle(NULL, B_F2_S(32.f));
    else
        batch_sprite(sprite->base.name);

    glm_ortho(0.f, (float)width, (float)height, 0.f, -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_colors(editor->current_def->colors);

    if (sprite == NULL) {
        batch_pos(B_F3_XY(width - 80.f, height - 80.f));
        batch_rectangle(NULL, B_F2_S(64.f));

        return;
    }

    const float scale = 64.f / sprite->size[1];
    batch_pos(B_F3_XY(width - 16.f - ((sprite->size[0] + sprite->offset[0]) * scale),
        height - 16.f - ((sprite->size[1] + sprite->offset[1]) * scale)));
    batch_scale(B_F2_S(scale));
    batch_sprite(sprite->base.name);

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
        const EditorDef* def = (EditorDef*)TinyMapGet(&editor->defs, folder->defs[i]);
        if (def != NULL && ImGui_MenuItem(def->name))
            editor->current_def = def;
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

        const EditorDef* next = NULL;
        if (ImGui_IsKeyPressed(ImGuiKey_Q) && editor->current_def != NULL)
            next = (EditorDef*)TinyMapGet(&editor->defs, editor->current_def->previous);
        if (ImGui_IsKeyPressed(ImGuiKey_E) && editor->current_def != NULL)
            next = (EditorDef*)TinyMapGet(&editor->defs, editor->current_def->next);
        if (next != NULL)
            editor->current_def = next;
    }

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
            !ImGui_IsKeyDown(ImGuiKey_LeftShift));
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

        ImGui_EndMainMenuBar();
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
