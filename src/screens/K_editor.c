#include <SDL3/SDL_platform_defines.h>

#include "K_interface.h"

#ifndef SDL_PLATFORM_EMSCRIPTEN

#include "K_video.h"

static void start(const void* secret, size_t secret_size) {
    (void)secret;
    (void)secret_size;
}

static void end() {}

static void draw() {
    clear_color(B_F4_VALUE(0.5f));

    int width = 0, height = 0;
    get_resolution(&width, &height);

    mat4 oproj = GLM_MAT4_IDENTITY_INIT;
    get_projection_matrix(oproj);

    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    glm_ortho(0.f, (float)width, (float)height, 0.f, -16000.f, 16000.f, proj);
    set_projection_matrix(proj);
    apply_matrices();

    batch_reset();

    set_projection_matrix(oproj);
    apply_matrices();
}

const ScreenTable TAB_EDITOR = {
    .start = start,
    .end = end,
    .draw = draw,
};

#else

const ScreenTable TAB_EDITOR = {};

#endif // SDL_PLATFORM_EMSCRIPTEN
