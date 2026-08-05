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
#include "K_net.h"
#include "K_string.h"
#include "K_video.h"

typedef struct {
    char label[256];
    char tracks[GSTR_TRACK_END - GSTR_TRACK_START + 1][256];
    char warps[MAX_GAME_WARPS][256];
    char secrets[GSTR_SECRET_END - GSTR_SECRET_START + 1][256];

    Bool ambush;
    unsigned int flags;

    int size[2], bounds[4];
    int time;
} EditorLevelState;

typedef struct {
    const char* error;
    EditorLevelState level;
} EditorState;

extern SDL_Window* WINDOW;

static EditorState* editor = NULL;

static void clear_editor_state() {
    // Clean up current state

    // Nullify entire state
    SDL_zerop(editor);

    // Level
    EditorLevelState* level = &editor->level;
    level->size[0] = SCREEN_WIDTH;
    level->size[1] = SCREEN_HEIGHT;
    level->bounds[2] = SCREEN_WIDTH;
    level->bounds[3] = SCREEN_HEIGHT;
    level->time = -1;
}

static void open_level(void* userdata, const char* const* files, int filter) {
    (void)userdata;
    (void)filter;

    ASSUME(files && *files, "No file to open");

    yyjson_read_err error = {0};
    yyjson_doc* json
        = yyjson_read_file(*files, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, &error);
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

    EditorLevelState* elevel = &editor->level;

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

static void open_level_dialog() {
    SDL_DialogFileFilter filter = {0};
    filter.name = "JSON file";
    filter.pattern = "json";
    SDL_ShowOpenFileDialog(open_level, NULL, WINDOW, &filter, 1, NULL, FALSE);
}

static void save_level(void* userdata, const char* const* files, int filter) {
    (void)userdata;
    (void)filter;

    ASSUME(files && *files, "No file to save");

    yyjson_mut_doc* json = yyjson_mut_doc_new(NULL);

    yyjson_mut_val* root = yyjson_mut_obj(json);
    yyjson_mut_doc_set_root(json, root);

    const EditorLevelState* elevel = &editor->level;

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

    const char* filename = *files;
    const size_t len = SDL_strlen(filename);
    if (len < 5 || SDL_strcmp(filename + len - 5, ".json") != 0)
        filename = fmt("%s.json", filename);

    yyjson_write_err error = {0};
    if (!yyjson_mut_write_file(filename, json, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, NULL, &error))
        editor->error = error.msg;

    yyjson_mut_doc_free(json);
}

static void save_level_dialog() {
    SDL_DialogFileFilter filter = {0};
    filter.name = "JSON file";
    filter.pattern = "json";
    SDL_ShowSaveFileDialog(save_level, NULL, WINDOW, &filter, 1, NULL);
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
    clear_editor_state();
}

static void end() {
    cImGui_ImplOpenGL3_Shutdown();
    cImGui_ImplSDL3_Shutdown();
    ImGui_DestroyContext(NULL);

    SDL_free(editor);

    rediscover_levels();
    recalculate_game_hash();
    update_net_game_id();
}

static void event(const SDL_Event* event) {
    cImGui_ImplSDL3_ProcessEvent(event);
}

static void draw() {
    clear_color(B_F4_VALUE(0.5f));

    int width = 0, height = 0;
    get_resolution(&width, &height);

    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    glm_ortho(0.f, (float)width, (float)height, 0.f, -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_reset();
    // DRAW MARKERS HERE
}

static void draw_ui() {
    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplOpenGL3_NewFrame();
    ImGui_NewFrame();

    if (ImGui_BeginMainMenuBar()) {
        if (ImGui_BeginMenu(LFMT("edit_file"))) {
            if (ImGui_MenuItem(LFMT("edit_new")))
                clear_editor_state();

            if (ImGui_MenuItem(LFMT("edit_open")))
                open_level_dialog();

            if (ImGui_MenuItem(LFMT("edit_save_as")))
                save_level_dialog();

            ImGui_Separator();

            if (ImGui_MenuItem(LFMT("edit_exit")))
                set_screen(SCR_MENU, NULL, 0);

            ImGui_EndMenu();
        }

        if (ImGui_BeginMenu(LFMT("edit_level"))) {
            EditorLevelState* elevel = &editor->level;

            if (ImGui_CollapsingHeader(LFMT("edit_strings"), 0)) {
                ImGui_InputText(LFMT("edit_label"), elevel->label, sizeof(elevel->label), 0);

                if (ImGui_TreeNode(LFMT("edit_tracks"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->tracks); i++) {
                        ImGui_InputText(
                            LFMT("edit_track", 'd', i + 1), elevel->tracks[i], sizeof(elevel->tracks[i]), 0);
                    }

                    ImGui_TreePop();
                    ImGui_Spacing();
                }

                if (ImGui_TreeNode(LFMT("edit_warps"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->warps); i++)
                        ImGui_InputText(LFMT("edit_warp", 'd', i + 1), elevel->warps[i], sizeof(elevel->warps[i]), 0);

                    ImGui_TreePop();
                    ImGui_Spacing();
                }

                if (ImGui_TreeNode(LFMT("edit_secrets"))) {
                    for (size_t i = 0; i < SDL_arraysize(elevel->secrets); i++) {
                        ImGui_InputText(
                            LFMT("edit_secret", 'd', i + 1), elevel->secrets[i], sizeof(elevel->secrets[i]), 0);
                    }

                    ImGui_TreePop();
                    ImGui_Spacing();
                }
            }

            if (ImGui_CollapsingHeader(LFMT("edit_constants"), 0)) {}

            if (ImGui_CollapsingHeader(LFMT("edit_flags"), 0)) {
                ImGui_CheckboxFlagsUintPtr(LFMT("edit_hardcore"), &elevel->flags, GF_HARDCORE);
                ImGui_CheckboxFlagsUintPtr(LFMT("edit_lost_map"), &elevel->flags, GF_LOST_MAP);
                ImGui_CheckboxFlagsUintPtr(LFMT("edit_funny_tanks"), &elevel->flags, GF_FUNNY_TANKS);
                ImGui_Checkbox(LFMT("edit_ambush"), (bool*)&elevel->ambush);
            }

            ImGui_Separator();

            ImGui_InputInt2(LFMT("edit_size"), elevel->size, 0);
            ImGui_InputInt4(LFMT("edit_bounds"), elevel->bounds, 0);
            ImGui_Separator();
            ImGui_InputInt(LFMT("edit_time"), &elevel->time);

            ImGui_EndMenu();
        }

        ImGui_MenuItem(LFMT("edit_markers"));

        ImGui_EndMainMenuBar();
    }

    if (editor->error != NULL)
        ImGui_OpenPopup(LFMT("edit_error"), 0);
    if (ImGui_BeginPopupModal(LFMT("edit_error"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui_Text("%s", LFMT(editor->error));
        ImGui_Spacing();

        if (ImGui_Button(LFMT("edit_ok"))) {
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
