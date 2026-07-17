#include <SDL3/SDL_platform_defines.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <glad/gles2.h>
#include <SDL3/SDL_opengles2.h>
#else
#include <glad/gl.h>
#include <SDL3/SDL_opengl.h>
#endif

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "K_cmd.h"
#include "K_log.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#define BBMOD_VERSION_MAJOR 3
#define BBMOD_VERSION_MINOR 4

#define HARD_BATCH_FUNC(type, state)                                                                                   \
    void batch_##state(type state) {                                                                                   \
        if (batch.state != state)                                                                                      \
            submit_batch();                                                                                            \
                                                                                                                       \
        batch.state = state;                                                                                           \
    }

typedef struct {
    GLuint fbo, texture[SURF_SIZE];
} SurfaceInternal;

typedef struct {
    GLuint vao, vbo;
} TileBatchInternal;

typedef struct {
    const char* name;
    GLuint shader;
    GLint uniforms[UNI_SIZE];
} Shader;

typedef struct {
    Bool filter;
    Bool write_color[4];
    Bool test_depth, write_depth;
    Bool test_stencil;
    Bool flip[2], tile[2];
    Uint8 color[4][4];
    FontAlignment align[2];
    BlendMode blend;
    Uint8 stencil_mask;
    StencilFunction stencil_func;
    Uint8 stencil_func_ref, stencil_func_mask;
    StencilOperation stencil_op[3];
    GLuint texture;
    float pos[3], offset[3], scale[2], angle;
    float stencil[4], alpha_test;

    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    Vertex* vertices;
} VertexBatch;

SDL_Window* WINDOW = NULL;
static SDL_GLContext gpu = NULL;

static int window_width = SCREEN_WIDTH, window_height = SCREEN_HEIGHT;
static int framerate = TICKRATE;
static Bool vsync = FALSE;

#define SHD(idx, nm) [idx] = {.name = (nm), -1}
static Shader SHADERS[SH_SIZE] = {
    SHD(SH_MAIN, "main"),
};
#undef SHD
static Shader* current_shader = NULL;

static GLuint blank_texture = 0;
static TinyMap textures = {0}, sprites = {0}, fonts = {0};

static VertexBatch batch = {0};
static Surface* current_surface = NULL;

static mat4 model_matrix = GLM_MAT4_IDENTITY_INIT, view_matrix = GLM_MAT4_IDENTITY_INIT,
            projection_matrix = GLM_MAT4_IDENTITY_INIT, mvp_matrix = GLM_MAT4_IDENTITY_INIT;

static Uint64 last_frame_time = 0;

static VideoState* video_state = NULL;

#ifdef SDL_PLATFORM_EMSCRIPTEN
#define CHECK_GL_EXTENSION(ext)                                                                                        \
    EXPECT((ext), "Missing extension \"" #ext "\". At least OpenGL ES 3.0 with shader support is required.");
#else
#define CHECK_GL_EXTENSION(ext)                                                                                        \
    EXPECT((ext), "Missing extension \"" #ext "\". At least OpenGL 3.3 with shader support is required.");
#endif

void video_init(Bool force_shader) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef SDL_PLATFORM_EMSCRIPTEN
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    // Window
    WINDOW = SDL_CreateWindow(
#ifdef SDL_PLATFORM_EMSCRIPTEN
        NULL
#else
        GAME_NAME
#endif
        ,
        window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    EXPECT(WINDOW, "Failed to create window: %s", SDL_GetError());

    // OpenGL
    gpu = SDL_GL_CreateContext(WINDOW);
    EXPECT(gpu && SDL_GL_MakeCurrent(WINDOW, gpu), "Failed to create graphics context: %s", SDL_GetError());

#ifdef SDL_PLATFORM_EMSCRIPTEN
    set_vsync(TRUE);

    int version = gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress);
#else
    set_vsync(FALSE);

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
#endif
    EXPECT(version, "Failed to load OpenGL functions");

    INFO("GLAD version: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
#ifdef SDL_PLATFORM_EMSCRIPTEN
    EXPECT(GLAD_GL_ES_VERSION_3_0,
        "Unsupported OpenGL version. At least OpenGL ES 3.0 with framebuffer and shader support is required.");
#else
    EXPECT(GLAD_GL_VERSION_3_3,
        "Unsupported OpenGL version. At least OpenGL 3.3 with framebuffer and shader support is required.");

    if (force_shader) {
        WARN("Bypassing OpenGL support checks");
        goto vi_bypass;
    }

    CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_array_object);
    CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_buffer_object);
    CHECK_GL_EXTENSION(GLAD_GL_ARB_framebuffer_object);
    CHECK_GL_EXTENSION(GLAD_GL_ARB_shader_objects);
    CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_shader);
    CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_shader);
#endif

vi_bypass:
    INFO("OpenGL vendor: %s", glGetString(GL_VENDOR));
    INFO("OpenGL version: %s", glGetString(GL_VERSION));
    INFO("OpenGL renderer: %s", glGetString(GL_RENDERER));
    INFO("OpenGL shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Blank texture
    glGenTextures(1, &blank_texture);
    glBindTexture(GL_TEXTURE_2D, blank_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, B_WHITE);

    // Vertex batch
    glGenVertexArrays(1, &batch.vao);
    glBindVertexArray(batch.vao);

    batch.vertex_count = 0;
    batch.vertex_capacity = 3;
    batch.vertices = SDL_calloc(batch.vertex_capacity, sizeof(Vertex));
    EXPECT(batch.vertices, "Failed to allocate batch vertices");

    glGenBuffers(1, &batch.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

    // NOLINTBEGIN(performance-no-int-to-ptr)
    glEnableVertexAttribArray(VATT_POSITION);
    glVertexAttribPointer(VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(VATT_COLOR);
    glVertexAttribPointer(VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(VATT_UV);
    glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    // NOLINTEND(performance-no-int-to-ptr)

    // Shaders
    for (size_t i = 0; i < SH_SIZE; i++) {
        Shader* shader = &SHADERS[i];

        // Vertex
        size_t vertex_size = 0;
        const char* vertex_src = load_data_file(fmt("shaders/%s.vsh", shader->name), &vertex_size);
        EXPECT(vertex_src, "Failed to read vertex shader for \"%s\": %s", shader->name, SDL_GetError());

        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_src, &(GLint){(GLint)vertex_size});
        glCompileShader(vertex_shader);
        SDL_free((void*)vertex_src);

        GLint success = 0;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char error[256];
            glGetShaderInfoLog(vertex_shader, sizeof(error), NULL, error);
            FATAL("Failed to compile vertex shader for \"%s\": %s", shader->name, error);
        }

        // Fragment
        size_t fragment_size = 0;
        const char* fragment_src = load_data_file(fmt("shaders/%s.fsh", shader->name), &fragment_size);
        EXPECT(fragment_src, "Failed to read fragment shader for \"%s\": %s", shader->name, SDL_GetError());

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_src, &(GLint){(GLint)fragment_size});
        glCompileShader(fragment_shader);
        SDL_free((void*)fragment_src);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char error[256];
            glGetShaderInfoLog(fragment_shader, sizeof(error), NULL, error);
            FATAL("Failed to compile fragment shader for \"%s\": %s", shader->name, error);
        }

        // Program
        shader->shader = glCreateProgram();
        glAttachShader(shader->shader, vertex_shader);
        glAttachShader(shader->shader, fragment_shader);
        glBindAttribLocation(shader->shader, VATT_POSITION, "i_position");
        glBindAttribLocation(shader->shader, VATT_COLOR, "i_color");
        glBindAttribLocation(shader->shader, VATT_UV, "i_uv");
        glLinkProgram(shader->shader);

        glGetProgramiv(shader->shader, GL_LINK_STATUS, &success);
        if (!success) {
            char error[256];
            glGetProgramInfoLog(shader->shader, sizeof(error), NULL, error);
            FATAL("Failed to link shaders for \"%s\": %s", shader->name, error);
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

// Uniforms
#define GET_UNIFORM(idx, name) shader->uniforms[idx] = glGetUniformLocation(shader->shader, name)
        GET_UNIFORM(UNI_MVP, "u_mvp");
        GET_UNIFORM(UNI_TEXTURE, "u_texture");
        GET_UNIFORM(UNI_ALPHA_TEST, "u_alpha_test");
        GET_UNIFORM(UNI_STENCIL, "u_stencil");
#undef GET_UNIFORM
    }

    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    set_shader(SH_MAIN);
    glDepthFunc(GL_LEQUAL);

    glm_ortho(0.f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.f, -16000.f, 16000.f, projection_matrix);
    glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
    glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    batch_reset_hard();
    last_frame_time = SDL_GetTicksNS();
}

#undef CHECK_GL_EXTENSION

void video_teardown() {
    glDeleteVertexArrays(1, &batch.vao);
    glDeleteBuffers(1, &batch.vbo);
    SDL_free(batch.vertices);

    glDeleteTextures(1, &blank_texture);
    FreeTinyMap(&textures);
    FreeTinyMap(&sprites);
    FreeTinyMap(&fonts);

    for (size_t i = 0; i < SH_SIZE; i++)
        glDeleteProgram(SHADERS[i].shader);

    SDL_GL_DestroyContext(gpu);
    SDL_DestroyWindow(WINDOW);
}

void start_drawing() {
    glm_mat4_identity(model_matrix);
    glm_mat4_identity(view_matrix);
    glm_ortho(0.f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.f, -16000.f, 16000.f, projection_matrix);
    glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
    glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
    glViewport(0, 0, window_width, window_height);
}

void start_drawing_ui() {
    submit_batch();

    const float ww = (float)window_width, wh = (float)window_height;

    const float scalew = ww / (float)SCREEN_WIDTH, scaleh = wh / (float)SCREEN_HEIGHT;
    const float scale = SDL_min(scalew, scaleh);

    const float left = ((ww - ((float)SCREEN_WIDTH * scale)) / -2.f) / scale,
                top = ((wh - ((float)SCREEN_HEIGHT * scale)) / -2.f) / scale;
    const float right = left + (ww / scale), bottom = top + (wh / scale);

    glm_mat4_identity(model_matrix);
    glm_mat4_identity(view_matrix);
    glm_ortho(left, right, bottom, top, -16000.f, 16000.f, projection_matrix);
    glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
    glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
}

void stop_drawing() {
    submit_batch();
    SDL_GL_SwapWindow(WINDOW);
}

void limit_framerate() {
    if (framerate <= 0)
        return;

    const Uint64 next_frame_time = last_frame_time + (1000000000 / framerate);
    const Uint64 current_frame_time = SDL_GetTicksNS();
    if (current_frame_time < next_frame_time) {
        SDL_DelayPrecise(next_frame_time - current_frame_time);
        last_frame_time = next_frame_time;
    } else {
        last_frame_time = current_frame_time;
    }
}

Bool window_maximized() {
    return get_fullscreen() || (SDL_GetWindowFlags(WINDOW) & SDL_WINDOW_MAXIMIZED);
}

#define FOCUS_IMPOSSIBLE (SDL_WINDOW_OCCLUDED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_NOT_FOCUSABLE)
#define HAS_FOCUS                                                                                                      \
    (SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_KEYBOARD_GRABBED)
Bool window_focused() {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(WINDOW);
    return !(flags & FOCUS_IMPOSSIBLE) && (flags & HAS_FOCUS);
}

// =======
// DISPLAY
// =======

void get_resolution(int* width, int* height) {
    SDL_GetWindowSizeInPixels(WINDOW, width, height);
}

void set_resolution(int width, int height, Bool center) {
    SDL_SetWindowSize(WINDOW, (width <= 0) ? SCREEN_WIDTH : width, (height <= 0) ? SCREEN_HEIGHT : height);
    SDL_SyncWindow(WINDOW);

    if (center) {
        SDL_SetWindowPosition(WINDOW, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_SyncWindow(WINDOW);
    }

    get_resolution(&window_width, &window_height);
}

Bool get_fullscreen() {
    return SDL_GetWindowFlags(WINDOW) & SDL_WINDOW_FULLSCREEN;
}

void set_fullscreen(Bool fullscreen) {
    SDL_SetWindowFullscreen(WINDOW, fullscreen);
    SDL_SyncWindow(WINDOW);
    SDL_RestoreWindow(WINDOW);
    SDL_SyncWindow(WINDOW);
    get_resolution(&window_width, &window_height);
}

int get_framerate() {
    return framerate;
}

void set_framerate(int fps) {
    framerate = fps;
}

Bool get_vsync() {
    return vsync;
}

void set_vsync(Bool vs) {
    if (!vs || !SDL_GL_SetSwapInterval(-1))
        SDL_GL_SetSwapInterval(vs);
    vsync = vs;
}

// =====
// BASIC
// =====

void clear_color(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void clear_depth(float depth) {
    glClearDepthf(depth);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void clear_stencil(Uint8 stencil) {
    glClearStencil(stencil);
    glClear(GL_STENCIL_BUFFER_BIT);
}

// =======
// SHADERS
// =======

void set_shader(ShaderType idx) {
    Shader* new_shader = &SHADERS[idx];
    if (current_shader == new_shader)
        return;

    submit_batch();
    current_shader = new_shader;
    glUseProgram(current_shader->shader);
    set_int_uniform(UNI_TEXTURE, GL_TEXTURE0);
}

void set_int_uniform(UniformType idx, int x) {
    glUniform1i(current_shader->uniforms[idx], x);
}

void set_float_uniform(UniformType idx, float x) {
    glUniform1f(current_shader->uniforms[idx], x);
}

void set_vec2_uniform(UniformType idx, const float x[2]) {
    glUniform2fv(current_shader->uniforms[idx], 1, x);
}

void set_vec3_uniform(UniformType idx, const float x[3]) {
    glUniform3fv(current_shader->uniforms[idx], 1, x);
}

void set_vec4_uniform(UniformType idx, const float x[4]) {
    glUniform4fv(current_shader->uniforms[idx], 1, x);
}

void set_mat4_uniform(UniformType idx, const float x[4][4]) {
    glUniformMatrix4fv(current_shader->uniforms[idx], 1, GL_FALSE, (const float*)x);
}

// ======
// ASSETS
// ======

static void nuke_texture(void* ptr) {
    Texture* texture = ptr;
    glDeleteTextures(1, texture->internal);
    SDL_free(texture->internal);
    SDL_free((void*)texture->base.name);
}

ASSET_SRC(textures, Texture, texture);

void load_texture(const char* name, AssetKeepLevel keep) {
    CHECK_ASSET(textures);

    Texture texture = {0};

    SDL_Surface* surface = SDL_LoadPNG_IO(stream_data_file(fmt("textures/%s.png", name), NULL), TRUE);
    ASSUME(surface, "Failed to load texture \"%s\": %s", name, SDL_GetError());

    texture.base.name = SDL_strdup(name);
    EXPECT(texture.base.name, "Failed to allocate name for texture \"%s\"", name);
    texture.base.keep = keep;
    texture.size[0] = surface->w;
    texture.size[1] = surface->h;

    texture.internal = SDL_calloc(1, sizeof(GLuint));
    EXPECT(texture.internal, "Failed to allocate texture handle");

    glGenTextures(1, texture.internal);
    glBindTexture(GL_TEXTURE_2D, *(GLuint*)texture.internal);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    if (surface->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface* temp = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (temp == NULL) {
            WTF("Failed to convert texture \"%s\": %s", name, SDL_GetError());
            SDL_free((void*)texture.base.name);
            return;
        }

        surface = temp;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_DestroySurface(surface);

    TinyDictPut(&textures, name, &texture, sizeof(texture))->cleanup = nuke_texture;
}

static void nuke_sprite(void* ptr) {
    Sprite* sprite = ptr;
    SDL_free((void*)sprite->base.name);
}

ASSET_SRC(sprites, Sprite, sprite);

void load_sprite(const char* name, AssetKeepLevel keep) {
    CHECK_ASSET(sprites);

    const char* tname = NULL;

    yyjson_doc* json = load_data_json(fmt("textures/%s.json", name));
    yyjson_val* root = NULL;
    if (json != NULL) {
        root = yyjson_doc_get_root(json);
        if (!yyjson_is_obj(root)) {
            WTF("Failed to load sprite \"%s\", expected object got %s", name, yyjson_get_type_desc(root));
            yyjson_doc_free(json);
            return;
        }

        tname = yyjson_get_str(yyjson_obj_get(root, "texture"));
        if (tname == NULL)
            tname = name;
    } else {
        tname = name;
    }

    const TinyHash tkey = StHashStr(tname);
    Texture* texture = (Texture*)TinyMapGet(&textures, tkey);
    if (texture == NULL) {
        load_texture(tname, keep);
        texture = (Texture*)TinyMapGet(&textures, tkey);
        if (texture == NULL) {
            WTF("Failed to load sprite \"%s\", invalid texture \"%s\"", name, tname);
            yyjson_doc_free(json);
            return;
        }
    } else {
        if (texture->base.keep < keep)
            texture->base.keep = keep;
    }

    Sprite sprite = {0};

    sprite.base.name = SDL_strdup(name);
    EXPECT(sprite.base.name, "Failed to allocate name for sprite \"%s\"", name);
    sprite.base.keep = keep;

    sprite.texture_key = tkey;

    yyjson_val* jarray = yyjson_obj_get(root, "size");
    if (yyjson_arr_size(jarray) < 2) {
        sprite.size[0] = (float)texture->size[0];
        sprite.size[1] = (float)texture->size[1];
    } else {
        sprite.size[0] = (float)yyjson_get_num(yyjson_arr_get(jarray, 0));
        sprite.size[1] = (float)yyjson_get_num(yyjson_arr_get(jarray, 1));
    }

    jarray = yyjson_obj_get(root, "offset");
    sprite.offset[0] = (float)yyjson_get_num(yyjson_arr_get(jarray, 0));
    sprite.offset[1] = (float)yyjson_get_num(yyjson_arr_get(jarray, 1));

    jarray = yyjson_obj_get(root, "uvs");
    if (yyjson_arr_size(jarray) < 4) {
        sprite.uvs[2] = sprite.uvs[3] = 1.f;
    } else {
        const float width = (float)texture->size[0], height = (float)texture->size[1];
        sprite.uvs[0] = (float)yyjson_get_num(yyjson_arr_get(jarray, 0)) / width;
        sprite.uvs[1] = (float)yyjson_get_num(yyjson_arr_get(jarray, 1)) / height;
        sprite.uvs[2] = (float)yyjson_get_num(yyjson_arr_get(jarray, 2)) / width;
        sprite.uvs[3] = (float)yyjson_get_num(yyjson_arr_get(jarray, 3)) / height;
    }

    yyjson_doc_free(json);

    TinyDictPut(&sprites, name, &sprite, sizeof(sprite))->cleanup = nuke_sprite;
}

static void nuke_font(void* ptr) {
    Font* font = ptr;
    FreeTinyMap(&font->glyphs);
    SDL_free((void*)font->base.name);
}

ASSET_SRC(fonts, Font, font);

void load_font(const char* name, AssetKeepLevel keep) {
    CHECK_ASSET(fonts);

    yyjson_doc* json = load_data_json(fmt("fonts/%s.json", name));
    ASSUME(json, "Font \"%s\" not found", name);

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        WTF("Font \"%s\" fail, expected object got %s", name, yyjson_get_type_desc(root));
        yyjson_doc_free(json);
        return;
    }

    Font font = {0};

    font.base.name = SDL_strdup(name);
    EXPECT(font.base.name, "Failed to allocate name for font \"%s\"", name);
    font.base.keep = keep;

    yyjson_val* jval = yyjson_obj_get(root, "size");
    const float size = yyjson_is_num(jval) ? (float)yyjson_get_num(jval) : 1.f;

    size_t i = 0, n = 0;
    yyjson_val *key = NULL, *value = NULL;
    jval = yyjson_obj_get(root, "glyphs");
    yyjson_obj_foreach(jval, i, n, key, value) {
        const char* tname = yyjson_get_str(key);
        if (tname == NULL || !yyjson_is_obj(value))
            continue;

        const TinyHash tkey = StHashStr(tname);
        Texture* texture = (Texture*)TinyMapGet(&textures, tkey);
        if (texture == NULL) {
            load_texture(tname, keep);
            texture = (Texture*)TinyMapGet(&textures, tkey);
        } else {
            if (texture->base.keep < keep)
                texture->base.keep = keep;
        }
        if (texture == NULL) {
            WTF("Font \"%s\" has invalid texture \"%s\"", name, tname);
            continue;
        }

        size_t gi = 0, gn = 0;
        yyjson_val *gkey = NULL, *gvalue = NULL;
        yyjson_obj_foreach(value, gi, gn, gkey, gvalue) {
            const char* gname = yyjson_get_str(gkey);
            if (gname == NULL || !yyjson_is_obj(gvalue))
                continue;

            const char* dummy = gname;
            Uint32 gid = SDL_StepUTF8(&dummy, NULL);
            if (TinyMapGet(&font.glyphs, gid) != NULL) {
                WARN("Font \"%s\" already has glyph \"%s\"", name, gname);
                continue;
            }

            if (!yyjson_is_obj(gvalue)) {
                WTF("Expected font \"%s\" glyph \"%s\" as object, got %s", name, gname, yyjson_get_type_desc(gvalue));
                continue;
            }

            Glyph glyph = {0};
            glyph.texture_key = tkey;

            yyjson_val* jarray = yyjson_obj_get(gvalue, "bounds");
            glyph.bounds[0] = (float)yyjson_get_num(yyjson_arr_get(jarray, 0)) / size;
            glyph.bounds[1] = (float)yyjson_get_num(yyjson_arr_get(jarray, 1)) / size;
            glyph.bounds[2] = (float)yyjson_get_num(yyjson_arr_get(jarray, 2)) / size;
            glyph.bounds[3] = (float)yyjson_get_num(yyjson_arr_get(jarray, 3)) / size;

            const float width = (float)texture->size[0], height = (float)texture->size[1];
            jarray = yyjson_obj_get(gvalue, "uvs");
            glyph.uvs[0] = (float)yyjson_get_num(yyjson_arr_get(jarray, 0)) / width;
            glyph.uvs[1] = (float)yyjson_get_num(yyjson_arr_get(jarray, 1)) / height;
            glyph.uvs[2] = (float)yyjson_get_num(yyjson_arr_get(jarray, 2)) / width;
            glyph.uvs[3] = (float)yyjson_get_num(yyjson_arr_get(jarray, 3)) / height;

            glyph.advance = (float)yyjson_get_num(yyjson_obj_get(gvalue, "advance")) / size;

            TinyMapPut(&font.glyphs, gid, &glyph, sizeof(glyph));
        }
    }

    yyjson_doc_free(json);

    TinyDictPut(&fonts, name, &font, sizeof(font))->cleanup = nuke_font;
}

// =====
// BATCH
// =====

void batch_pos(const float pos[3]) {
    batch.pos[0] = pos[0];
    batch.pos[1] = pos[1];
    batch.pos[2] = pos[2];
}

void batch_offset(const float offset[3]) {
    batch.offset[0] = offset[0];
    batch.offset[1] = offset[1];
    batch.offset[2] = offset[2];
}

void batch_scale(const float scale[2]) {
    batch.scale[0] = scale[0];
    batch.scale[1] = scale[1];
}

void batch_angle(const float angle) {
    batch.angle = angle;
}

void batch_color(const Uint8 color[4]) {
    SDL_memcpy(batch.color[0], color, sizeof(batch.color[0]));
    SDL_memcpy(batch.color[1], color, sizeof(batch.color[1]));
    SDL_memcpy(batch.color[2], color, sizeof(batch.color[2]));
    SDL_memcpy(batch.color[3], color, sizeof(batch.color[3]));
}

void batch_colors(const Uint8 colors[4][4]) {
    SDL_memcpy(batch.color, colors, sizeof(batch.color));
}

void batch_stencil(const float stencil[4]) {
    if (batch.stencil[0] != stencil[0] || batch.stencil[1] != stencil[1] || batch.stencil[2] != stencil[2]
        || batch.stencil[3] != stencil[3])
    {
        submit_batch();
    }

    batch.stencil[0] = stencil[0];
    batch.stencil[1] = stencil[1];
    batch.stencil[2] = stencil[2];
    batch.stencil[3] = stencil[3];
}

void batch_flip(const Bool flip[2]) {
    batch.flip[0] = flip[0];
    batch.flip[1] = flip[1];
}

void batch_align(const FontAlignment align[2]) {
    batch.align[0] = align[0];
    batch.align[1] = align[1];
}

static HARD_BATCH_FUNC(GLuint, texture);

void batch_tile(const Bool tile[2]) {
    if (batch.tile[0] != tile[0] || batch.tile[1] != tile[1])
        submit_batch();

    batch.tile[0] = tile[0];
    batch.tile[1] = tile[1];
}

HARD_BATCH_FUNC(Bool, filter);
HARD_BATCH_FUNC(float, alpha_test);

void batch_blend(BlendMode blend) {
    if (batch.blend != blend)
        submit_batch();

    batch.blend = blend;
    switch (batch.blend) {
    default:
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
        break;
    case BM_ADD:
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
        break;
    case BM_SUBTRACT:
        glBlendFuncSeparate(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE);
        break;
    case BM_MULTIPLY:
        glBlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
        break;
    }
}

void batch_write_color(Bool r, Bool g, Bool b, Bool a) {
    if (batch.write_color[0] != r || batch.write_color[1] != g || batch.write_color[2] != b
        || batch.write_color[3] != a)
    {
        submit_batch();
    }

    batch.write_color[0] = r;
    batch.write_color[1] = g;
    batch.write_color[2] = b;
    batch.write_color[3] = a;
    glColorMask(batch.write_color[0] ? GL_TRUE : GL_FALSE, batch.write_color[1] ? GL_TRUE : GL_FALSE,
        batch.write_color[2] ? GL_TRUE : GL_FALSE, batch.write_color[3] ? GL_TRUE : GL_FALSE);
}

HARD_BATCH_FUNC(Bool, test_depth);

void batch_write_depth(Bool write_depth) {
    if (batch.write_depth != write_depth)
        submit_batch();

    batch.write_depth = write_depth;
    glDepthMask(batch.write_depth ? GL_TRUE : GL_FALSE);
}

HARD_BATCH_FUNC(Bool, test_stencil);

void batch_stencil_mask(Uint8 stencil_mask) {
    if (batch.stencil_mask != stencil_mask)
        submit_batch();

    batch.stencil_mask = stencil_mask;
    glStencilMask(batch.stencil_mask);
}

void batch_stencil_func(StencilFunction stencil_func, Uint8 stencil_func_ref, Uint8 stencil_func_mask) {
    if (batch.stencil_func != stencil_func || batch.stencil_func_ref != stencil_func_ref
        || batch.stencil_func_mask != stencil_func_mask)
    {
        submit_batch();
    }

    batch.stencil_func = stencil_func;
    batch.stencil_func_mask = stencil_func_mask;
    batch.stencil_func_ref = stencil_func_ref;

    GLenum func = GL_ALWAYS;
    switch (batch.stencil_func) {
    default:
        break;
    case STF_NEVER:
        func = GL_NEVER;
        break;
    case STF_LESS:
        func = GL_LESS;
        break;
    case STF_LESS_OR_EQUAL:
        func = GL_LEQUAL;
        break;
    case STF_EQUAL:
        func = GL_EQUAL;
        break;
    case STF_GREATER_OR_EQUAL:
        func = GL_GEQUAL;
        break;
    case STF_GREATER:
        func = GL_GREATER;
        break;
    }
    glStencilFunc(func, batch.stencil_func_ref, batch.stencil_func_mask);
}

static GLenum sto_to_glenum(StencilOperation op) {
    switch (op) {
    default:
        return GL_KEEP;
    case STO_ZERO:
        return GL_ZERO;
    case STO_REPLACE:
        return GL_REPLACE;
    case STO_INCREMENT:
        return GL_INCR;
    case STO_INCREMENT_WRAP:
        return GL_INCR_WRAP;
    case STO_DECREMENT:
        return GL_DECR;
    case STO_DECREMENT_WRAP:
        return GL_DECR_WRAP;
    case STO_INVERT:
        return GL_INVERT;
    }
}

void batch_stencil_op(StencilOperation stencil_fail, StencilOperation depth_fail, StencilOperation depth_pass) {
    if (batch.stencil_op[0] != stencil_fail || batch.stencil_op[1] != depth_fail || batch.stencil_op[2] != depth_pass)
        submit_batch();

    batch.stencil_op[0] = stencil_fail;
    batch.stencil_op[1] = depth_fail;
    batch.stencil_op[2] = depth_pass;
    glStencilOp(
        sto_to_glenum(batch.stencil_op[0]), sto_to_glenum(batch.stencil_op[1]), sto_to_glenum(batch.stencil_op[2]));
}

/// Resets the batch.
void batch_reset() {
    batch_pos(B_ORIGIN);
    batch_offset(B_ORIGIN);
    batch_scale(B_SIZE(1.f));
    batch_angle(0.f);
    batch_color(B_WHITE);
    batch_flip(B_NO_FLIP);
    batch_align(B_TOP_LEFT);
}

/// Fully resets the batch, slower since it can break batches.
void batch_reset_hard() {
    batch_reset();
    batch_texture(blank_texture);
    batch_tile(B_NO_TILE);
    batch_filter(TRUE);
    batch_alpha_test(0.f);
    batch_stencil(B_NO_STENCIL);
    batch_blend(BM_NORMAL);
    batch_write_color(TRUE, TRUE, TRUE, TRUE);
    batch_test_depth(TRUE);
    batch_write_depth(TRUE);
    batch_test_stencil(FALSE);
    batch_stencil_mask(255);
    batch_stencil_func(STF_ALWAYS, 0, 255);
    batch_stencil_op(STO_KEEP, STO_KEEP, STO_KEEP);
}

/// Adds a sprite to the batch.
void batch_sprite(const char* name) {
    const Sprite* sprite = get_sprite(name);
    if (sprite == NULL)
        return;

    const Texture* texture = get_texture_key(sprite->texture_key);
    batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);

    // Position
    const float w = sprite->size[0] * batch.scale[0], h = sprite->size[1] * batch.scale[1];

    const float xoffs = (sprite->offset[0] + batch.offset[0]) * batch.scale[0],
                yoffs = (sprite->offset[1] + batch.offset[1]) * batch.scale[1];

    const float x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
    const float x2 = x1 + w, y2 = y1 + h;
    const float z = batch.offset[2];

    vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
    if (batch.angle != 0.f) {
        glm_vec2_rotate(p1, batch.angle, p1);
        glm_vec2_rotate(p2, batch.angle, p2);
        glm_vec2_rotate(p3, batch.angle, p3);
        glm_vec2_rotate(p4, batch.angle, p4);
    }
    glm_vec3_add(batch.pos, p1, p1);
    glm_vec3_add(batch.pos, p2, p2);
    glm_vec3_add(batch.pos, p3, p3);
    glm_vec3_add(batch.pos, p4, p4);

    // UVs
    const float u1 = sprite->uvs[batch.flip[0] ? 2 : 0], v1 = sprite->uvs[batch.flip[1] ? 3 : 1];
    const float u2 = sprite->uvs[batch.flip[0] ? 0 : 2], v2 = sprite->uvs[batch.flip[1] ? 1 : 3];

    // Vertices
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
    batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

/// Adds a surface to the vertex batch.
void batch_surface(const Surface* surface) {
    if (surface == NULL)
        return;
    EXPECT(!surface->active, "Drawing an active surface?");

    const GLuint stex = ((SurfaceInternal*)surface->internal)->texture[SURF_COLOR];
    batch_texture((stex == 0) ? blank_texture : stex);

    // Position
    const float w = (float)surface->size[0] * batch.scale[0], h = (float)surface->size[1] * batch.scale[1];

    const float xoffs = batch.offset[0] * batch.scale[0], yoffs = batch.offset[1] * batch.scale[1];

    const float x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
    const float x2 = x1 + w, y2 = y1 + h;
    const float z = batch.offset[2];

    vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
    if (batch.angle != 0.f) {
        glm_vec2_rotate(p1, batch.angle, p1);
        glm_vec2_rotate(p2, batch.angle, p2);
        glm_vec2_rotate(p3, batch.angle, p3);
        glm_vec2_rotate(p4, batch.angle, p4);
    }
    glm_vec3_add(batch.pos, p1, p1);
    glm_vec3_add(batch.pos, p2, p2);
    glm_vec3_add(batch.pos, p3, p3);
    glm_vec3_add(batch.pos, p4, p4);

    // UVs
    const float u1 = batch.flip[0], v1 = batch.flip[1];
    const float u2 = (float)(!batch.flip[0]), v2 = (float)(!batch.flip[1]);

    // Vertices
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
    batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

/// Adds a rectangle to the batch.
void batch_rectangle(const char* name, const float size[2]) {
    const Texture* texture = get_texture(name);
    batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);

    float tw = 1.f, th = 1.f;
    if (texture != NULL)
        tw = (float)texture->size[0] * batch.scale[0], th = (float)texture->size[1] * batch.scale[1];

    // Position
    const float w = size[0] * batch.scale[0], h = size[1] * batch.scale[1];
    const float xoffs = batch.offset[0] * batch.scale[0], yoffs = batch.offset[1] * batch.scale[1];
    const float x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
    const float x2 = x1 + w, y2 = y1 + h;
    const float z = batch.offset[2];

    vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
    if (batch.angle != 0.f) {
        glm_vec2_rotate(p1, batch.angle, p1);
        glm_vec2_rotate(p2, batch.angle, p2);
        glm_vec2_rotate(p3, batch.angle, p3);
        glm_vec2_rotate(p4, batch.angle, p4);
    }
    glm_vec3_add(batch.pos, p1, p1);
    glm_vec3_add(batch.pos, p2, p2);
    glm_vec3_add(batch.pos, p3, p3);
    glm_vec3_add(batch.pos, p4, p4);

    // UVs
    float u1 = 0.f, v1 = 0.f, u2 = 1.f, v2 = 1.f;
    if (batch.tile[0])
        u2 = (batch.flip[0] ? -1.f : 1.f) * ((x2 - x1) / tw);
    else
        u1 = batch.flip[0], u2 = (float)(!batch.flip[0]);
    if (batch.tile[1])
        v2 = (batch.flip[1] ? -1.f : 1.f) * ((y2 - y1) / th);
    else
        v1 = batch.flip[1], v2 = (float)(!batch.flip[1]);

    // Vertices
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
    batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
    batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
    batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

/// Adds a circle to the batch.
void batch_circle(const char* name, float radius) {
    const Texture* texture = get_texture(name);
    batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);

    const float x = batch.pos[0] - (batch.offset[0] * batch.scale[0]),
                y = batch.pos[1] - (batch.offset[1] * batch.scale[1]), z = batch.pos[2];
    const float xrad = radius * batch.scale[0], yrad = radius * batch.scale[1];
    static const Uint8 n = 64;
    for (Uint8 i = 0; i < n; i++) {
        const float forward = (SDL_PI_F * 2.f) * ((float)i / (float)n);
        const float up = (SDL_PI_F * 2.f) * ((float)(i + 1) / (float)n);

        const float nx1 = SDL_cosf(forward), ny1 = SDL_sinf(forward);
        const float nx2 = SDL_cosf(up), ny2 = SDL_sinf(up);

        batch_vertex(B_XYZ(x, y, z), batch.color[0], B_UV(0.5f, 0.5f));
        batch_vertex(B_XYZ(x + (nx1 * xrad), y + (ny1 * yrad), z), batch.color[1],
            B_UV(0.5f + (nx1 * 0.5f), 0.5f + (ny1 * 0.5f)));
        batch_vertex(B_XYZ(x + (nx2 * xrad), y + (ny2 * yrad), z), batch.color[1],
            B_UV(0.5f + (nx2 * 0.5f), 0.5f + (ny2 * 0.5f)));
    }
}

static float string_width_fast(const Font* font, float size, const char* str) {
    float width = 0.f;

    float cx = 0.f;
    const float xscale = size * batch.scale[0];

    size_t bytes = SDL_strlen(str);
    while (bytes > 0) {
        Uint32 gid = SDL_StepUTF8(&str, &bytes);

        // Special/invalid characters
        if (gid == '\r')
            continue;
        if (gid == '\n') {
            cx = 0.f;
            continue;
        }
        if (SDL_isspace((int)gid))
            gid = ' ';

        // Valid glyph
        const Glyph* glyph = (Glyph*)TinyMapGet(&font->glyphs, gid);
        if (glyph == NULL)
            continue;

        const float advance = glyph->advance * xscale, extend = glyph->bounds[2] * xscale;
        const float should_extend = cx + SDL_max(advance, extend);
        width = SDL_max(width, should_extend);

        cx += advance;
    }

    return width;
}

float string_width(const char* name, float size, const char* str) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return 0.f;

    return string_width_fast(font, size, str);
}

static float string_height_fast(float size, const char* str) {
    float height = 0.f;
    const float gh = size * batch.scale[1];

    size_t bytes = SDL_strlen(str);
    while (bytes > 0)
        if (SDL_StepUTF8(&str, &bytes) == '\n')
            height += gh;

    return height + gh;
}

float string_height(const char* name, float size, const char* str) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return size;

    return string_height_fast(size, str);
}

/// Adds a string to the batch.
void batch_string(const char* name, float size, const char* str) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return;

    // Origin
    float ox = batch.pos[0] - (batch.offset[0] * batch.scale[0]);
    float oy = batch.pos[1] - (batch.offset[1] * batch.scale[1]);
    float oz = batch.pos[2] - batch.offset[2];

    // Horizontal alignment
    switch (batch.align[0]) {
    case FA_CENTER:
        ox -= string_width_fast(font, size, str) * 0.5f;
        break;
    case FA_RIGHT:
        ox -= string_width_fast(font, size, str);
        break;
    default:
        break;
    }

    // Vertical alignment
    switch (batch.align[1]) {
    case FA_MIDDLE:
        oy -= string_height_fast(size, str) * 0.5f;
        break;
    case FA_BOTTOM:
        oy -= string_height_fast(size, str);
        break;
    default:
        break;
    }

    float cx = ox, cy = oy;
    const float xscale = size * batch.scale[0], yscale = size * batch.scale[1];

    size_t bytes = SDL_strlen(str);
    while (bytes > 0) {
        Uint32 gid = SDL_StepUTF8(&str, &bytes);

        // Special/invalid characters
        if (gid == '\r')
            continue;
        if (gid == '\n') {
            cx = ox;
            cy += yscale;
            continue;
        }
        if (SDL_isspace((int)gid))
            gid = ' ';

        // Valid glyph
        const Glyph* glyph = (Glyph*)TinyMapGet(&font->glyphs, gid);
        if (glyph == NULL)
            continue;
        const Texture* texture = get_texture_key(glyph->texture_key);
        batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);

        const float x1 = cx + (glyph->bounds[0] * xscale), y1 = cy + (glyph->bounds[1] * yscale);
        const float x2 = cx + (glyph->bounds[2] * xscale), y2 = cy + (glyph->bounds[3] * yscale);
        batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));
        batch_vertex(B_XYZ(x1, y1, oz), batch.color[0], B_UV(glyph->uvs[0], glyph->uvs[1]));
        batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
        batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
        batch_vertex(B_XYZ(x2, y2, oz), batch.color[3], B_UV(glyph->uvs[2], glyph->uvs[3]));
        batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));

        if (bytes > 0)
            cx += glyph->advance * xscale;
    }
}

static float string_width_wrap_fast(const Font* font, float size, const char* str, float wrap) {
    float width = 0.f;

    float cx = 0.f;
    const float xscale = size * batch.scale[0];

    // https://github.com/raysan5/raylib/blob/master/examples/text/text_rectangle_bounds.c
    const size_t bytes = SDL_strlen(str);
    Bool measure = TRUE;
    Sint32 start_pos = -1, end_pos = -1;

    for (Sint32 i = 0; i < bytes; i++) {
        const char* adv = &str[i];
        size_t advbytes = bytes - i;
        const size_t last_advbytes = advbytes;

        const size_t gid = SDL_StepUTF8(&adv, &advbytes);
        const Bool space = SDL_isspace((int)gid);

        const Glyph* glyph = (gid == '\n') ? NULL : (Glyph*)TinyMapGet(&font->glyphs, space ? ' ' : gid);
        float gadv = (glyph == NULL) ? 0.f : (glyph->advance * xscale);

        i += (Sint32)(last_advbytes - advbytes) - 1;

        if (measure) {
            if (space)
                end_pos = i;

            if ((cx + gadv) > wrap) {
                if (end_pos <= 0)
                    end_pos = i;
                if (i == end_pos)
                    end_pos -= 1;
                if ((start_pos + 1) == end_pos)
                    end_pos = i - 1;
                measure = FALSE;
            } else if ((i + 1) == bytes) {
                end_pos = i;
                measure = FALSE;
            } else if (gid == '\n') {
                measure = FALSE;
            }

            if (!measure) {
                cx = 0.f;
                i = start_pos;
                gadv = 0.f;
            }
        } else if (i == end_pos) {
            cx = 0.f;
            start_pos = end_pos;
            end_pos = 0;
            measure = TRUE;
        }

        const float extend = (glyph == NULL) ? 0.f : (glyph->bounds[2] * xscale);
        const float should_extend = cx + SDL_max(gadv, extend);
        width = SDL_max(width, should_extend);

        if (cx > 0.f || !space)
            cx += gadv;
    }

    return width;
}

float string_width_wrap(const char* name, float size, const char* str, float wrap) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return size;

    return string_width_wrap_fast(font, size, str, wrap);
}

static float string_height_wrap_fast(const Font* font, float size, const char* str, float wrap) {
    float cx = 0.f, cy = 0.f;
    const float xscale = size * batch.scale[0], yscale = size * batch.scale[1];

    // https://github.com/raysan5/raylib/blob/master/examples/text/text_rectangle_bounds.c
    const size_t bytes = SDL_strlen(str);
    Bool measure = TRUE;
    Sint32 start_pos = -1, end_pos = -1;

    for (Sint32 i = 0; i < bytes; i++) {
        const char* adv = &str[i];
        size_t advbytes = bytes - i;
        const size_t last_advbytes = advbytes;

        const size_t gid = SDL_StepUTF8(&adv, &advbytes);
        const Bool space = SDL_isspace((int)gid);

        const Glyph* glyph = (gid == '\n') ? NULL : (Glyph*)TinyMapGet(&font->glyphs, space ? ' ' : gid);
        float gwidth = (glyph == NULL) ? 0.f : (glyph->advance * xscale);

        i += (Sint32)(last_advbytes - advbytes) - 1;

        if (measure) {
            if (space)
                end_pos = i;

            if ((cx + gwidth) > wrap) {
                if (end_pos <= 0)
                    end_pos = i;
                if (i == end_pos)
                    end_pos -= 1;
                if ((start_pos + 1) == end_pos)
                    end_pos = i - 1;
                measure = FALSE;
            } else if ((i + 1) == bytes) {
                end_pos = i;
                measure = FALSE;
            } else if (gid == '\n') {
                measure = FALSE;
            }

            if (!measure) {
                cx = 0.f;
                i = start_pos;
                gwidth = 0.f;
            }
        } else if (i == end_pos) {
            cx = 0.f;
            cy += yscale;
            start_pos = end_pos;
            end_pos = 0;
            measure = TRUE;
        }

        if (cx > 0.f || !space)
            cx += gwidth;
    }

    return cy;
}

float string_height_wrap(const char* name, float size, const char* str, float wrap) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return size;

    return string_height_wrap_fast(font, size, str, wrap);
}

void batch_string_wrap(const char* name, float size, const char* str, float wrap) {
    const Font* font = get_font(name);
    if (font == NULL || str == NULL)
        return;

    // Origin
    float ox = batch.pos[0] - (batch.offset[0] * batch.scale[0]);
    float oy = batch.pos[1] - (batch.offset[1] * batch.scale[1]);
    const float oz = batch.pos[2] - batch.offset[2];

    // Horizontal alignment
    switch (batch.align[0]) {
    case FA_CENTER:
        ox -= string_width_wrap_fast(font, size, str, wrap) * 0.5f;
        break;
    case FA_RIGHT:
        ox -= string_width_wrap_fast(font, size, str, wrap);
        break;
    default:
        break;
    }

    // Vertical alignment
    switch (batch.align[1]) {
    case FA_MIDDLE:
        oy -= string_height_wrap_fast(font, size, str, wrap) * 0.5f;
        break;
    case FA_BOTTOM:
        oy -= string_height_wrap_fast(font, size, str, wrap);
        break;
    default:
        break;
    }

    float cx = 0.f, cy = 0.f;
    const float xscale = size * batch.scale[0], yscale = size * batch.scale[1];

    // https://github.com/raysan5/raylib/blob/master/examples/text/text_rectangle_bounds.c
    const size_t bytes = SDL_strlen(str);
    Bool measure = TRUE;
    Sint32 start_pos = -1, end_pos = -1;

    for (Sint32 i = 0; i < bytes; i++) {
        const char* adv = &str[i];
        size_t advbytes = bytes - i;
        const size_t last_advbytes = advbytes;

        const size_t gid = SDL_StepUTF8(&adv, &advbytes);
        const Bool space = SDL_isspace((int)gid);

        const Glyph* glyph = (gid == '\n') ? NULL : (Glyph*)TinyMapGet(&font->glyphs, space ? ' ' : gid);
        float gwidth = (glyph == NULL) ? 0.f : (glyph->advance * xscale);
        if (glyph != NULL) {
            const Texture* texture = get_texture_key(glyph->texture_key);
            batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);
        }

        i += (Sint32)(last_advbytes - advbytes) - 1;

        if (measure) {
            if (space)
                end_pos = i;

            if ((cx + gwidth) > wrap) {
                if (end_pos <= 0)
                    end_pos = i;
                if (i == end_pos)
                    end_pos -= 1;
                if ((start_pos + 1) == end_pos)
                    end_pos = i - 1;
                measure = FALSE;
            } else if ((i + 1) == bytes) {
                end_pos = i;
                measure = FALSE;
            } else if (gid == '\n') {
                measure = FALSE;
            }

            if (!measure) {
                cx = 0.f;
                i = start_pos;
                gwidth = 0.f;
            }
        } else {
            if (glyph != NULL) {
                const float acx = ox + cx, acy = oy + cy;
                const float x1 = acx + (glyph->bounds[0] * xscale), y1 = acy + (glyph->bounds[1] * yscale);
                const float x2 = acx + (glyph->bounds[2] * xscale), y2 = acy + (glyph->bounds[3] * yscale);
                batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));
                batch_vertex(B_XYZ(x1, y1, oz), batch.color[0], B_UV(glyph->uvs[0], glyph->uvs[1]));
                batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
                batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
                batch_vertex(B_XYZ(x2, y2, oz), batch.color[3], B_UV(glyph->uvs[2], glyph->uvs[3]));
                batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));
            }

            if (i == end_pos) {
                cx = 0.f;
                cy += yscale;
                start_pos = end_pos;
                end_pos = 0;
                measure = TRUE;
            }
        }

        if (cx > 0.f || !space)
            cx += gwidth;
    }
}

static void apply_batch() {
    if (current_surface != NULL && current_surface->enabled[SURF_DEPTH]) {
        (batch.test_depth ? glEnable : glDisable)(GL_DEPTH_TEST);
        (batch.test_stencil ? glEnable : glDisable)(GL_STENCIL_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
    }
}

void batch_primitive(const char* name) {
    batch_primitive_direct(get_texture(name));
}

void batch_primitive_direct(const Texture* texture) {
    batch_texture((texture == NULL) ? blank_texture : *(GLuint*)texture->internal);
}

void batch_primitive_surface(Surface* surface) {
    batch_texture((surface == NULL || surface->active) ? blank_texture
                                                       : ((SurfaceInternal*)surface->internal)->texture[SURF_COLOR]);
}

/// Adds a vertex to the batch.
void batch_vertex(const float pos[3], const Uint8 color[4], const float uv[2]) {
    if (batch.vertex_count >= batch.vertex_capacity) {
        submit_batch();

        const size_t new_size = batch.vertex_capacity * 2;
        EXPECT(new_size >= batch.vertex_capacity, "Capacity overflow in vertex batch");
        batch.vertices = SDL_realloc(batch.vertices, new_size * sizeof(Vertex));
        EXPECT(batch.vertices, "Out of memory for vertex batch");
        batch.vertex_capacity = new_size;

        glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);
    }

    batch.vertices[batch.vertex_count++] = (Vertex){
        .position = {pos[0], pos[1], pos[2]},
        .color = {color[0], color[1], color[2], color[3]},
        .uv = {uv[0], uv[1]},
    };
}

/// Dumps the vertex batch on your screen.
void submit_batch() {
    if (batch.vertex_count <= 0)
        return;

    glBindVertexArray(batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_count), batch.vertices);

    // Apply texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, batch.texture);
    const Sint32 filter = (batch.filter && CLIENT.texture_filter) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, batch.tile[0] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, batch.tile[1] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    set_float_uniform(UNI_ALPHA_TEST, batch.alpha_test);
    set_vec4_uniform(UNI_STENCIL, batch.stencil);
    set_mat4_uniform(UNI_MVP, *get_mvp_matrix());

    apply_batch();

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
    batch.vertex_count = 0;
}

// ========
// MATRICES
// ========

void get_model_matrix(mat4 dest) {
    glm_mat4_copy((current_surface == NULL) ? model_matrix : current_surface->model_matrix, dest);
}

void get_view_matrix(mat4 dest) {
    glm_mat4_copy((current_surface == NULL) ? view_matrix : current_surface->view_matrix, dest);
}

void get_projection_matrix(mat4 dest) {
    if (current_surface != NULL) {
        glm_mat4_copy(current_surface->projection_matrix, dest);
        dest[1][1] *= -1.f;
        return;
    }
    glm_mat4_copy(projection_matrix, dest);
}

const mat4* get_mvp_matrix() {
    return (current_surface == NULL) ? &mvp_matrix : &current_surface->mvp_matrix;
}

void set_model_matrix(mat4 matrix) {
    glm_mat4_copy(matrix, (current_surface == NULL) ? model_matrix : current_surface->model_matrix);
}

void set_view_matrix(mat4 matrix) {
    glm_mat4_copy(matrix, (current_surface == NULL) ? view_matrix : current_surface->view_matrix);
}

void set_projection_matrix(mat4 matrix) {
    if (current_surface != NULL) {
        glm_mat4_copy(matrix, current_surface->projection_matrix);
        current_surface->projection_matrix[1][1] *= -1.f;
        return;
    }
    glm_mat4_copy(matrix, projection_matrix);
}

void apply_matrices() {
    submit_batch();
    if (current_surface == NULL) {
        glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
        glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
    } else {
        glm_mat4_mul(current_surface->view_matrix, current_surface->model_matrix, current_surface->mvp_matrix);
        glm_mat4_mul(current_surface->projection_matrix, current_surface->mvp_matrix, current_surface->mvp_matrix);
    }
}

// ========
// SURFACES
// ========

Surface* create_surface(Uint16 width, Uint16 height, Bool color, Bool depth) {
    Surface* surface = SDL_calloc(1, sizeof(*surface));
    EXPECT(surface, "Failed to allocate surface");

    glm_mat4_identity(surface->model_matrix);
    glm_mat4_identity(surface->view_matrix);
    glm_ortho(0.f, (float)width, 0.f, (float)height, -16000.f, 16000.f, surface->projection_matrix);
    glm_mat4_mul(surface->view_matrix, surface->model_matrix, surface->mvp_matrix);
    glm_mat4_mul(surface->projection_matrix, surface->mvp_matrix, surface->mvp_matrix);

    surface->enabled[SURF_COLOR] = color;
    surface->enabled[SURF_DEPTH] = depth;
    surface->size[0] = SDL_clamp(width, 1, SDL_MAX_UINT16);
    surface->size[1] = SDL_clamp(height, 1, SDL_MAX_UINT16);

    SurfaceInternal* sdata = SDL_calloc(1, sizeof(*sdata));
    EXPECT(sdata, "Failed to allocate surface data");
    surface->internal = sdata;

    return surface;
}

/// Nukes the surface.
void destroy_surface(Surface* surface) {
    if (surface == NULL)
        return;
    dispose_surface(surface);
    SDL_free(surface->internal);
    SDL_free(surface);
}

static void dispose_surface_buffer(Surface* surface, SurfaceAttribute idx) {
    SurfaceInternal* sdata = surface->internal;
    if (sdata->texture[idx] == 0)
        return;

    glDeleteTextures(1, &sdata->texture[idx]);
    sdata->texture[idx] = 0;
}

static void check_surface_buffer(
    Surface* surface, SurfaceAttribute idx, Sint32 internal_format, Uint32 format, Uint32 type, Uint32 attachment) {
    if (surface->enabled[idx]) {
        if (((SurfaceInternal*)surface->internal)->texture[idx] != 0)
            return;
    } else {
        dispose_surface_buffer(surface, idx);
        return;
    }

    SurfaceInternal* sdata = surface->internal;
    glGenTextures(1, &sdata->texture[idx]);
    glBindFramebuffer(GL_FRAMEBUFFER, sdata->fbo);
    glBindTexture(GL_TEXTURE_2D, sdata->texture[idx]);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, surface->size[0], surface->size[1], 0, format, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, sdata->texture[idx], 0);
}

/// Checks the surface for a valid framebuffer.
void check_surface(Surface* surface) {
    SurfaceInternal* sdata = surface->internal;
    if (sdata->fbo == 0)
        glGenFramebuffers(1, &sdata->fbo);
    check_surface_buffer(surface, SURF_COLOR, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0);
    check_surface_buffer(
        surface, SURF_DEPTH, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT);
}

/// Nukes the surface's framebuffer.
void dispose_surface(Surface* surface) {
    EXPECT(!surface->active, "Disposing an active surface?");

    SurfaceInternal* sdata = surface->internal;
    if (sdata->fbo != 0) {
        glDeleteFramebuffers(1, &sdata->fbo);
        sdata->fbo = 0;
    }

    dispose_surface_buffer(surface, SURF_COLOR);
    dispose_surface_buffer(surface, SURF_DEPTH);
}

void resize_surface(Surface* surface, Uint16 width, Uint16 height) {
    EXPECT(!surface->active, "Resizing an active surface?");

    if (surface->size[0] == width && surface->size[1] == height)
        return;

    dispose_surface(surface);
    surface->size[0] = SDL_clamp(width, 1, SDL_MAX_UINT16);
    surface->size[1] = SDL_clamp(height, 1, SDL_MAX_UINT16);
}

/// Pushes an INACTIVE surface onto the stack.
void push_surface(Surface* surface) {
    EXPECT(surface, "Pushing a null surface?");
    EXPECT(current_surface != surface, "Pushing the current surface?");
    submit_batch();

    if (surface == NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        glCullFace(GL_FRONT);
    } else {
        EXPECT(!surface->active, "Pushing an active surface?");
        surface->active = TRUE;
        surface->previous = current_surface;

        check_surface(surface);
        glBindFramebuffer(GL_FRAMEBUFFER, ((SurfaceInternal*)surface->internal)->fbo);
        glViewport(0, 0, surface->size[0], surface->size[1]);
        glCullFace(GL_BACK);
    }

    current_surface = surface;
}

/// Pops an ACTIVE surface off the stack.
void pop_surface() {
    EXPECT(current_surface, "Popping a null surface?");
    submit_batch();

    Surface* surface = current_surface->previous;
    current_surface->active = FALSE;
    current_surface->previous = NULL;

    if (surface == NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, window_width, window_height);
        glCullFace(GL_FRONT);
    } else {
        check_surface(surface);
        glBindFramebuffer(GL_FRAMEBUFFER, ((SurfaceInternal*)surface->internal)->fbo);
        glViewport(0, 0, surface->size[0], surface->size[1]);
        glCullFace(GL_BACK);
    }

    current_surface = surface;
}

// ========
// TILEMAPS
// ========

TileMap* create_tilemap() {
    TileMap* tilemap = SDL_calloc(1, sizeof(*tilemap));
    EXPECT(tilemap, "Failed to allocate tilemap");
    return tilemap;
}

void destroy_tilemap(TileMap* tilemap) {
    if (tilemap != NULL) {
        FreeTinyMap(&tilemap->batches);
        SDL_free(tilemap);
    }
}

static void nuke_tile_batch(void* ptr) {
    TileBatch* tile_batch = ptr;

    TileBatchInternal* tbdata = tile_batch->internal;
    glDeleteVertexArrays(1, &tbdata->vao);
    glDeleteBuffers(1, &tbdata->vbo);
    SDL_free(tbdata);

    SDL_free(tile_batch->vertices);
}

static void tile_batch_vertex(TileBatch* tile_batch, const float pos[3], const Uint8 color[4], const float uv[2]) {
    if (tile_batch->vertex_count >= tile_batch->vertex_capacity) {
        const size_t new_size = tile_batch->vertex_capacity * 2;
        EXPECT(new_size >= tile_batch->vertex_capacity, "Capacity overflow in tile batch");
        tile_batch->vertices = SDL_realloc(tile_batch->vertices, new_size * sizeof(Vertex));
        EXPECT(tile_batch->vertices, "Out of memory for tile batch");
        tile_batch->vertex_capacity = new_size;

        glBindBuffer(GL_ARRAY_BUFFER, ((TileBatchInternal*)tile_batch->internal)->vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * tile_batch->vertex_capacity), NULL, GL_STATIC_DRAW);
    }

    tile_batch->vertices[tile_batch->vertex_count++] = (Vertex){
        .position = {pos[0], pos[1], pos[2]},
        .color = {color[0], color[1], color[2], color[3]},
        .uv = {uv[0], uv[1]},
    };
    tile_batch->transparent |= color[3] < 255;
}

static void tile_batch_sprite(TileBatch* tile_batch, const Sprite* sprite, const float pos[3], const float size[2],
    const Bool flip[2], const Bool tile[2], const Uint8 colors[4][4]) {
    tile_batch->tile[0] |= tile[0];
    tile_batch->tile[1] |= tile[1];

    const float x = pos[0] - ((sprite == NULL || tile[0]) ? 0.f : sprite->offset[0]),
                y = pos[1] - ((sprite == NULL || tile[1]) ? 0.f : sprite->offset[1]);

    float w = 1.f, h = 1.f;
    if (size == NULL) {
        if (sprite != NULL) {
            w = sprite->size[0];
            h = sprite->size[1];
        }
    } else {
        w = size[0];
        h = size[1];
    }

    float u1 = 0.f, v1 = 0.f, u2 = 1.f, v2 = 1.f;
    if (sprite != NULL) {
        if (tile[0]) {
            *(flip[0] ? &u1 : &u2) = w / sprite->size[0];
        } else {
            u1 = sprite->uvs[flip[0] ? 2 : 0];
            u2 = sprite->uvs[flip[0] ? 0 : 2];
        }
        if (tile[1]) {
            *(flip[1] ? &v1 : &v2) = h / sprite->size[1];
        } else {
            v1 = sprite->uvs[flip[1] ? 3 : 1];
            v2 = sprite->uvs[flip[1] ? 1 : 3];
        }
    }

    const float x2 = x + w, y2 = y + h;
    tile_batch_vertex(tile_batch, B_XYZ(x, y2, pos[2]), colors[2], B_UV(u1, v2));
    tile_batch_vertex(tile_batch, B_XYZ(x, y, pos[2]), colors[0], B_UV(u1, v1));
    tile_batch_vertex(tile_batch, B_XYZ(x2, y, pos[2]), colors[1], B_UV(u2, v1));
    tile_batch_vertex(tile_batch, B_XYZ(x2, y, pos[2]), colors[1], B_UV(u2, v1));
    tile_batch_vertex(tile_batch, B_XYZ(x2, y2, pos[2]), colors[3], B_UV(u2, v2));
    tile_batch_vertex(tile_batch, B_XYZ(x, y2, pos[2]), colors[2], B_UV(u1, v2));
}

void add_tilemap(TileMap* tilemap, const char* name, const float pos[3], const float size[2], const Bool flip[2],
    const Bool tile[2], const Uint8 colors[4][4]) {
    if (tilemap == NULL)
        return;

    const TinyHash key = StHashStr(name);
    const Sprite* sprite = get_sprite_key(key);
    if (sprite == NULL && name != NULL) {
        WARN("Unknown sprite \"%s\"", name);
        return;
    }

    TileBatch* tile_batch = (TileBatch*)TinyMapGet(&tilemap->batches, key);
    if (tile_batch != NULL) {
        tile_batch_sprite(tile_batch, sprite, pos, size, flip, tile, colors);
        return;
    }

    tile_batch = &(TileBatch){0};
    tile_batch->texture_key = (sprite == NULL) ? key : sprite->texture_key;

    tile_batch->vertex_capacity = 6;
    tile_batch->vertices = SDL_calloc(tile_batch->vertex_capacity, sizeof(*tile_batch->vertices));
    EXPECT(tile_batch->vertices, "Failed to allocate tile batch vertices");

    TileBatchInternal* tbdata = SDL_calloc(1, sizeof(*tbdata));
    EXPECT(tbdata, "Failed to allocate tile batch data");

    glGenVertexArrays(1, &tbdata->vao);
    glBindVertexArray(tbdata->vao);

    glGenBuffers(1, &tbdata->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbdata->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * tile_batch->vertex_capacity), NULL, GL_STATIC_DRAW);

    // NOLINTBEGIN(performance-no-int-to-ptr)
    glEnableVertexAttribArray(VATT_POSITION);
    glVertexAttribPointer(VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(VATT_COLOR);
    glVertexAttribPointer(VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(VATT_UV);
    glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    // NOLINTEND(performance-no-int-to-ptr)

    tile_batch->internal = tbdata;

    tile_batch_sprite(tile_batch, sprite, pos, size, flip, tile, colors);
    TinyBucket* bucket = TinyMapPut(&tilemap->batches, key, tile_batch, sizeof(*tile_batch));
    bucket->cleanup = nuke_tile_batch;

    if (tilemap->first_batch == NULL) {
        tilemap->first_batch = bucket->data;
    } else {
        TileBatch* tb = tilemap->first_batch;
        while (tb != NULL) {
            if (tb->next == NULL) {
                tb->next = bucket->data;
                break;
            }

            tb = tb->next;
        }
    }
}

void read_tilemap(TileMap* tilemap, yyjson_val* jarray) {
    for (size_t i = 0, n = yyjson_arr_size(jarray); i < n; i++) {
        yyjson_val* jtile = yyjson_arr_get(jarray, i);
        if (!yyjson_is_obj(jtile))
            continue;

        const char* sprite = yyjson_get_str(yyjson_obj_get(jtile, "sprite"));
        load_sprite(sprite, AKL_NEVER);

        yyjson_val* jval = yyjson_obj_get(jtile, "pos");
        const float pos[3] = {
            (float)yyjson_get_num(yyjson_arr_get(jval, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jval, 1)),
            (float)yyjson_get_num(yyjson_arr_get(jval, 2)),
        };

        jval = yyjson_obj_get(jtile, "size");
        const Bool has_size = yyjson_is_arr(jval);
        const float size[2] = {
            (float)yyjson_get_num(yyjson_arr_get(jval, 0)),
            (float)yyjson_get_num(yyjson_arr_get(jval, 1)),
        };

        jval = yyjson_obj_get(jtile, "flip");
        const Bool flip[2] = {
            yyjson_get_bool(yyjson_arr_get(jval, 0)),
            yyjson_get_bool(yyjson_arr_get(jval, 1)),
        };

        jval = yyjson_obj_get(jtile, "tile");
        const Bool tile[2] = {
            yyjson_get_bool(yyjson_arr_get(jval, 0)),
            yyjson_get_bool(yyjson_arr_get(jval, 1)),
        };

        jval = yyjson_obj_get(jtile, "colors");
        Uint8 colors[4][4] = {
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255}
        };
        for (size_t j = 0, n = yyjson_arr_size(jval); j < n && j < 4; j++) {
            yyjson_val* const jcolor = yyjson_arr_get(jval, j);
            for (size_t k = 0; k < 4; k++)
                colors[j][k] = yyjson_get_uint(yyjson_arr_get(jcolor, k));
        }

        add_tilemap(tilemap, sprite, pos, has_size ? size : NULL, flip, tile, colors);
    }
}

void draw_tilemap(const TileMap* tilemap) {
    if (tilemap == NULL)
        return;

    submit_batch();

    set_float_uniform(UNI_ALPHA_TEST, batch.alpha_test);
    set_vec4_uniform(UNI_STENCIL, batch.stencil);
    set_mat4_uniform(UNI_MVP, *get_mvp_matrix());
    apply_batch();

    const Sint32 filter = (batch.filter && CLIENT.texture_filter) ? GL_LINEAR : GL_NEAREST;

    for (const TileBatch* tile_batch = tilemap->first_batch; tile_batch != NULL; tile_batch = tile_batch->next) {
        const TileBatchInternal* tbdata = tile_batch->internal;

        glBindVertexArray(tbdata->vao);
        glBindBuffer(GL_ARRAY_BUFFER, tbdata->vbo);
        glBufferSubData(
            GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * tile_batch->vertex_count), tile_batch->vertices);

        // Apply texture
        const Texture* texture = get_texture_key(tile_batch->texture_key);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (texture == NULL) ? blank_texture : *(GLuint*)texture->internal);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tile_batch->tile[0] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tile_batch->tile[1] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tile_batch->vertex_count);
    }
}

// =====
// STATE
// =====

void start_video_state() {
    video_state = SDL_calloc(1, sizeof(*video_state));
    EXPECT(video_state, "Failed to allocate video state");

    video_state->tilemap = create_tilemap();
}

void nuke_video_state() {
    if (video_state == NULL)
        return;

    destroy_tilemap(video_state->tilemap);
    SDL_free(video_state);
    video_state = NULL;
}

void tick_video_state() {}

VideoState* videostate() {
    return video_state;
}
