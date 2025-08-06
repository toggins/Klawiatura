#include <SDL3/SDL_timer.h>
#include <SDL3_image/SDL_image.h>

#define CGLM_ALL_UNALIGNED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include "cglm/cglm.h" // IWYU pragma: keep

#include "K_game.h"
#include "K_log.h"
#include "K_video.h"

#define CHECK_GL_EXTENSION(ext)                                                                                        \
    if (!(ext))                                                                                                        \
        FATAL("Missing OpenGL extension: " #ext "\nAt least OpenGL 3.3 with shader support is required.");

static SDL_Window* window = NULL;
static SDL_GLContext gpu = NULL;

static uint64_t draw_time = 0;

static GLuint shader = 0;
static struct Uniforms uniforms = {-1};

#define TEXOFFS(i, nm, xoffs, yoffs)                                                                                   \
    [(i)] = {                                                                                                          \
        .name = (nm),                                                                                                  \
        .texture = 0,                                                                                                  \
        .size = {0, 0},                                                                                                \
        .offset = {(xoffs), (yoffs)},                                                                                  \
    }
#define TEXTURE(i, nm) TEXOFFS(i, nm, 0, 0)

static struct Texture textures[] = {
    TEXTURE(TEX_NULL, NULL),

    TEXTURE(TEX_CLOUD1, "props/cloud1"),
    TEXTURE(TEX_CLOUD2, "props/cloud2"),
    TEXTURE(TEX_CLOUD3, "props/cloud3"),
    TEXOFFS(TEX_BUSH1, "props/bush1", 0, -2),
    TEXOFFS(TEX_BUSH2, "props/bush2", 0, -4),
    TEXOFFS(TEX_BUSH3, "props/bush3", 0, -4),

    TEXOFFS(TEX_PLAYER, "player", 16, 16),

    TEXOFFS(TEX_BULLET, "bullet", 7, 7),
    TEXOFFS(TEX_BULLET_HIT1, "bullet_hit1", 8, 7),
    TEXOFFS(TEX_BULLET_HIT2, "bullet_hit2", 12, 13),
    TEXOFFS(TEX_BULLET_HIT3, "bullet_hit3", 16, 15),
};

static vec2 camera = {HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT};
static mat4 mvp = GLM_MAT4_IDENTITY;
static struct VertexBatch batch = {0};

void video_init(bool bypass_shader) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Window
    window = SDL_CreateWindow("Klawiatura", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    if (window == NULL)
        FATAL("Window fail: %s", SDL_GetError());

    // OpenGL
    gpu = SDL_GL_CreateContext(window);
    if (gpu == NULL || !SDL_GL_MakeCurrent(window, gpu))
        FATAL("GPU fail: %s", SDL_GetError());
    SDL_GL_SetSwapInterval(0);

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0)
        FATAL("Failed to load OpenGL functions");
    INFO("GLAD version: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    if (!GLAD_GL_VERSION_3_3)
        FATAL("Unsupported OpenGL version\nAt least OpenGL 3.3 with framebuffer and shader support is required.");
    if (bypass_shader) {
        INFO("! Bypassing shader support");
    } else {
        CHECK_GL_EXTENSION(GLAD_GL_ARB_shader_objects);
        CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_shader);
        CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_shader);
        CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_program);
        CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_program);
    }

    INFO("OpenGL vendor: %s", glGetString(GL_VENDOR));
    INFO("OpenGL version: %s", glGetString(GL_VERSION));
    INFO("OpenGL renderer: %s", glGetString(GL_RENDERER));
    INFO("OpenGL shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Blank texture
    load_texture(TEX_NULL);

    // Vertex batch
    glGenVertexArrays(1, &(batch.vao));
    glBindVertexArray(batch.vao);
    glEnableVertexArrayAttrib(batch.vao, VATT_POSITION);
    glVertexArrayAttribFormat(batch.vao, VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3);
    glEnableVertexArrayAttrib(batch.vao, VATT_COLOR);
    glVertexArrayAttribFormat(batch.vao, VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLubyte) * 4);
    glEnableVertexArrayAttrib(batch.vao, VATT_UV);
    glVertexArrayAttribFormat(batch.vao, VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);

    batch.vertex_count = 0;
    batch.vertex_capacity = 3;
    batch.vertices = SDL_malloc(batch.vertex_capacity * sizeof(struct Vertex));
    if (batch.vertices == NULL)
        FATAL("batch.vertices fail");

    glGenBuffers(1, &(batch.vbo));
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(struct Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(VATT_POSITION);
    glVertexAttribPointer(
        VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, position)
    );

    glEnableVertexAttribArray(VATT_COLOR);
    glVertexAttribPointer(
        VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, color)
    );

    glEnableVertexAttribArray(VATT_UV);
    glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, uv));

    batch.color[0] = batch.color[1] = batch.color[2] = batch.color[3] = 1;
    batch.texture = textures[TEX_NULL].texture;
    batch.alpha_test = 0.5f;

    batch.blend_src[0] = batch.blend_src[1] = GL_SRC_ALPHA;
    batch.blend_dest[0] = GL_ONE_MINUS_SRC_ALPHA;
    batch.blend_dest[1] = GL_ONE;

    batch.filter = false;

    // Shader
    static const GLchar* vertex_source = "#version 330 core\n"
                                         "layout (location = 0) in vec3 i_position;\n"
                                         "layout (location = 1) in vec4 i_color;\n"
                                         "layout (location = 2) in vec2 i_uv;\n"
                                         "\n"
                                         "out vec4 v_color;\n"
                                         "out vec2 v_uv;\n"
                                         "\n"
                                         "uniform mat4 u_mvp;\n"
                                         "\n"
                                         "void main() {\n"
                                         "   gl_Position = u_mvp * vec4(i_position, 1.0);\n"
                                         "   v_color = i_color;\n"
                                         "   v_uv = i_uv;\n"
                                         "}\n";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_source, NULL);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar error[1024];
        glGetShaderInfoLog(vertex_shader, sizeof(error), NULL, error);
        FATAL("Vertex shader fail: %s", error);
    }

    static const GLchar* fragment_source = "#version 330 core\n"
                                           "\n"
                                           "out vec4 o_color;\n"
                                           "\n"
                                           "in vec4 v_color;\n"
                                           "in vec2 v_uv;\n"
                                           "\n"
                                           "uniform sampler2D u_texture;\n"
                                           "uniform float u_alpha_test;\n"
                                           "\n"
                                           "void main() {\n"
                                           "   vec4 sample = texture(u_texture, v_uv);\n"
                                           "   if (u_alpha_test > 0.0) {\n"
                                           "       if (sample.a < u_alpha_test)\n"
                                           "           discard;\n"
                                           "       sample.a = 1.;\n"
                                           "   }\n"
                                           "\n"
                                           "   o_color = v_color * sample;\n"
                                           "}\n";

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_source, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar error[1024];
        glGetShaderInfoLog(fragment_shader, sizeof(error), NULL, error);
        FATAL("Fragment shader fail: %s", error);
    }

    shader = glCreateProgram();
    glAttachShader(shader, vertex_shader);
    glAttachShader(shader, fragment_shader);
    glBindAttribLocation(shader, VATT_POSITION, "i_position");
    glBindAttribLocation(shader, VATT_COLOR, "i_color");
    glBindAttribLocation(shader, VATT_UV, "i_uv");
    glLinkProgram(shader);

    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar error[1024];
        glGetProgramInfoLog(shader, sizeof(error), NULL, error);
        FATAL("Shader fail:\n%s", error);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    uniforms.mvp = glGetUniformLocation(shader, "u_mvp");
    uniforms.texture = glGetUniformLocation(shader, "u_texture");
    uniforms.alpha_test = glGetUniformLocation(shader, "u_alpha_test");

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}

void video_update() {
    draw_time = SDL_GetTicks();

    glm_ortho(
        camera[0] - HALF_SCREEN_WIDTH, camera[0] + HALF_SCREEN_WIDTH, camera[1] + HALF_SCREEN_HEIGHT,
        camera[1] - HALF_SCREEN_HEIGHT, -16000, 16000, mvp
    );

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_state();
    batch_vertex(16, 128, 0, 255, 0, 0, 255, 0, 0);
    batch_vertex(128, 16, 0, 0, 255, 0, 255, 0, 0);
    batch_vertex(16, 16, 0, 0, 0, 255, 255, 0, 0);
    submit_batch();
    SDL_GL_SwapWindow(window);
}

void video_teardown() {
    glDeleteProgram(shader);
    for (size_t i = 0; i < TEX_SIZE; i++)
        glDeleteTextures(1, &(textures[i].texture));
    glDeleteVertexArrays(1, &(batch.vao));
    glDeleteBuffers(1, &(batch.vbo));
    SDL_free(batch.vertices);

    SDL_GL_DestroyContext(gpu);
    SDL_DestroyWindow(window);
}

void load_texture(enum TextureIndices index) {
    struct Texture* texture = &(textures[index]);
    if (texture->texture != 0)
        return;

    if (texture->name == NULL) {
        texture->size[0] = 1;
        texture->size[1] = 1;

        glGenTextures(1, &(texture->texture));
        glBindTexture(GL_TEXTURE_2D, texture->texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const uint8_t[]){255, 255, 255, 255}
        );

        return;
    }

    static char path[1024];
    SDL_snprintf(path, sizeof(path), "%sdata/textures/%s.png", SDL_GetBasePath(), texture->name);

    SDL_Surface* surface = IMG_Load(path);
    if (surface == NULL)
        FATAL("Texture \"%s\" fail: %s", texture->name, SDL_GetError());

    texture->size[0] = surface->w;
    texture->size[1] = surface->h;

    glGenTextures(1, &(texture->texture));
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    GLint format;
    switch (surface->format) {
        default: {
            SDL_Surface* temp = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            if (temp == NULL)
                FATAL("Texture \"%s\" conversion fail: %s", texture->name, SDL_GetError());
            SDL_DestroySurface(surface);
            surface = temp;

            format = GL_RGBA8;
            break;
        }

        case SDL_PIXELFORMAT_RGB24:
            format = GL_RGB8;
            break;
        case SDL_PIXELFORMAT_RGB48:
            format = GL_RGB16;
            break;
        case SDL_PIXELFORMAT_RGB48_FLOAT:
            format = GL_RGB16F;
            break;
        case SDL_PIXELFORMAT_RGB96_FLOAT:
            format = GL_RGB32F;
            break;

        case SDL_PIXELFORMAT_RGBA32:
            format = GL_RGBA8;
            break;
        case SDL_PIXELFORMAT_RGBA64:
            format = GL_RGBA16;
            break;
        case SDL_PIXELFORMAT_RGBA64_FLOAT:
            format = GL_RGBA16F;
            break;
        case SDL_PIXELFORMAT_RGBA128_FLOAT:
            format = GL_RGBA32F;
            break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_DestroySurface(surface);
}

void submit_batch() {
    if (batch.vertex_count <= 0)
        return;

    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)mvp);

    glBindVertexArray(batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(struct Vertex) * batch.vertex_count), batch.vertices);

    // Apply texture
    glBindTexture(GL_TEXTURE_2D, batch.texture);
    const GLint filter = batch.filter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glUniform1i(uniforms.texture, 0);
    glUniform1f(uniforms.alpha_test, batch.alpha_test);

    // Apply blend mode
    glBlendFuncSeparate(batch.blend_src[0], batch.blend_dest[0], batch.blend_src[1], batch.blend_dest[1]);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
    batch.vertex_count = 0;
}

void batch_vertex(GLfloat x, GLfloat y, GLfloat z, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat u, GLfloat v) {
    if (batch.vertex_count >= batch.vertex_capacity) {
        submit_batch();

        const size_t new_size = batch.vertex_capacity * 2;
        if (new_size < batch.vertex_capacity)
            FATAL("Capacity overflow in vertex batch");
        batch.vertices = SDL_realloc(batch.vertices, new_size * sizeof(struct Vertex));
        if (batch.vertices == NULL)
            FATAL("Out of memory for vertex batch");

        batch.vertex_capacity = new_size;

        glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
        glBufferData(
            GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(struct Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW
        );
    }

    batch.vertices[batch.vertex_count++] = (struct Vertex){x,
                                                           y,
                                                           z,
                                                           (GLubyte)(batch.color[0] * (GLfloat)r),
                                                           (GLubyte)(batch.color[1] * (GLfloat)g),
                                                           (GLubyte)(batch.color[2] * (GLfloat)b),
                                                           (GLubyte)(batch.color[3] * (GLfloat)a),
                                                           u,
                                                           v};
}

void draw_sprite(enum TextureIndices index, const GLfloat pos[3], const bool flip[2]) {
    struct Texture* texture = &(textures[index]);
    GLuint tex = texture->texture;
    if (tex == 0)
        FATAL("Invalid texture index %u", index);
    if (batch.texture != tex)
        submit_batch();
    batch.texture = tex;

    const GLfloat x1 = pos[0] - (flip[0] ? ((GLfloat)(texture->size[0]) - (texture->offset[0])) : (texture->offset[0]));
    const GLfloat y1 = pos[1] - (flip[1] ? ((GLfloat)(texture->size[1]) - (texture->offset[1])) : (texture->offset[1]));
    const GLfloat x2 = x1 + (GLfloat)texture->size[0];
    const GLfloat y2 = y1 + (GLfloat)texture->size[1];
    const GLfloat z = pos[2];

    const GLfloat u1 = flip[0];
    const GLfloat v1 = flip[1];
    const GLfloat u2 = (GLfloat)(!flip[0]);
    const GLfloat v2 = (GLfloat)(!flip[1]);

    batch_vertex(x1, y2, z, 255, 255, 255, 255, u1, v2);
    batch_vertex(x1, y1, z, 255, 255, 255, 255, u1, v1);
    batch_vertex(x2, y1, z, 255, 255, 255, 255, u2, v1);
    batch_vertex(x2, y1, z, 255, 255, 255, 255, u2, v1);
    batch_vertex(x2, y2, z, 255, 255, 255, 255, u2, v2);
    batch_vertex(x1, y2, z, 255, 255, 255, 255, u1, v2);
}
