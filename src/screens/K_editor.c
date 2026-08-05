#include <SDL3/SDL_platform_defines.h>

#include "K_interface.h"

#ifndef SDL_PLATFORM_EMSCRIPTEN

#include <SDL3/SDL_video.h>

#include <dcimgui.h>
#include <dcimgui_impl_opengl3.h>
#include <dcimgui_impl_sdl3.h>

#include "K_video.h"

static void start(const void* secret, size_t secret_size) {
    (void)secret;
    (void)secret_size;

    CIMGUI_CHECKVERSION();

    ImGui_CreateContext(NULL);
    extern SDL_Window* WINDOW;
    extern SDL_GLContext GPU;
    cImGui_ImplSDL3_InitForOpenGL(WINDOW, GPU);
    cImGui_ImplOpenGL3_Init();
}

static void end() {
    cImGui_ImplOpenGL3_Shutdown();
    cImGui_ImplSDL3_Shutdown();
    ImGui_DestroyContext(NULL);
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
    batch_string("main", 24.f, "DUMMY");
}

static void draw_ui() {
    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplOpenGL3_NewFrame();
    ImGui_NewFrame();

    ImGui_ShowDemoWindow((bool*)&(Bool){TRUE});

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

#else

const ScreenTable TAB_EDITOR = {};

#endif // SDL_PLATFORM_EMSCRIPTEN
