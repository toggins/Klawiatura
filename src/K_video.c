#include <SDL3/SDL_timer.h>
#include <SDL3_image/SDL_image.h>

#define CGLM_ALL_UNALIGNED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include "cglm/cglm.h" // IWYU pragma: keep

#include "K_audio.h"
#include "K_file.h"
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

#define NULLTEX(i)                                                                                                     \
    [(i)] = {                                                                                                          \
        .name = NULL,                                                                                                  \
        .texture = 0,                                                                                                  \
        .size = {0, 0},                                                                                                \
        .offset = {0, 0},                                                                                              \
    }

#define TEXOFFS(i, nm, xoffs, yoffs)                                                                                   \
    [(i)] = {                                                                                                          \
        .name = "textures/" nm,                                                                                        \
        .texture = 0,                                                                                                  \
        .size = {0, 0},                                                                                                \
        .offset = {(xoffs), (yoffs)},                                                                                  \
    }

#define TEXTURE(i, nm) TEXOFFS(i, nm, 0, 0)

static struct Texture textures[] = {
    NULLTEX(TEX_NULL),

    TEXTURE(TEX_GRASS1, "tiles/grass1"),
    TEXTURE(TEX_GRASS2, "tiles/grass2"),
    TEXTURE(TEX_GRASS3, "tiles/grass3"),
    TEXTURE(TEX_GRASS4, "tiles/grass4"),
    TEXTURE(TEX_GRASS5, "tiles/grass5"),
    TEXTURE(TEX_GRASS6, "tiles/grass6"),
    TEXTURE(TEX_GRASS7, "tiles/grass7"),
    TEXTURE(TEX_GRASS8, "tiles/grass8"),
    TEXTURE(TEX_GRASS9, "tiles/grass9"),

    TEXTURE(TEX_SNOW1, "tiles/snow1"),
    TEXTURE(TEX_SNOW2, "tiles/snow2"),
    TEXTURE(TEX_SNOW3, "tiles/snow3"),
    TEXTURE(TEX_SNOW4, "tiles/snow4"),
    TEXTURE(TEX_SNOW5, "tiles/snow5"),
    TEXTURE(TEX_SNOW6, "tiles/snow6"),
    TEXTURE(TEX_SNOW7, "tiles/snow7"),
    TEXTURE(TEX_SNOW8, "tiles/snow8"),
    TEXTURE(TEX_SNOW9, "tiles/snow9"),

    TEXTURE(TEX_BLOCK1, "tiles/block1"),
    TEXTURE(TEX_BLOCK2, "tiles/block2"),
    TEXTURE(TEX_BLOCK3, "tiles/block3"),
    TEXTURE(TEX_BLOCK4, "tiles/block4"),

    TEXTURE(TEX_BRIDGE1, "tiles/bridge1"),
    TEXTURE(TEX_BRIDGE2, "tiles/bridge2"),
    TEXTURE(TEX_BRIDGE3, "tiles/bridge3"),

    TEXTURE(TEX_CLOUD1, "props/cloud1"),
    TEXTURE(TEX_CLOUD2, "props/cloud2"),
    TEXTURE(TEX_CLOUD3, "props/cloud3"),
    TEXOFFS(TEX_BUSH1, "props/bush1", 0, -2),
    TEXOFFS(TEX_BUSH2, "props/bush2", 0, -4),
    TEXOFFS(TEX_BUSH3, "props/bush3", 0, -4),
    TEXOFFS(TEX_BUSH_SNOW1, "props/bush_snow1", 0, 0),
    TEXOFFS(TEX_BUSH_SNOW2, "props/bush_snow2", 0, 0),
    TEXOFFS(TEX_BUSH_SNOW3, "props/bush_snow3", 0, 0),

    TEXOFFS(TEX_MARIO_DEAD, "mario/dead", 15, 30),

    TEXOFFS(TEX_MARIO_SMALL, "mario/small/idle", 10, 28),
    TEXOFFS(TEX_MARIO_SMALL_WALK1, "mario/small/walk1", 15, 30),
    TEXOFFS(TEX_MARIO_SMALL_WALK2, "mario/small/walk2", 11, 28),
    TEXOFFS(TEX_MARIO_SMALL_JUMP, "mario/small/jump", 15, 30),
    TEXOFFS(TEX_MARIO_SMALL_SWIM1, "mario/small/swim1", 15, 30),
    TEXOFFS(TEX_MARIO_SMALL_SWIM2, "mario/small/swim2", 15, 29),
    TEXOFFS(TEX_MARIO_SMALL_SWIM3, "mario/small/swim3", 15, 31),
    TEXOFFS(TEX_MARIO_SMALL_SWIM4, "mario/small/swim4", 15, 30),

    TEXOFFS(TEX_MARIO_BIG_GROW, "mario/big/grow", 12, 42),
    TEXOFFS(TEX_MARIO_BIG, "mario/big/idle", 15, 58),
    TEXOFFS(TEX_MARIO_BIG_WALK1, "mario/big/walk1", 16, 55),
    TEXOFFS(TEX_MARIO_BIG_WALK2, "mario/big/walk2", 16, 56),
    TEXOFFS(TEX_MARIO_BIG_JUMP, "mario/big/jump", 16, 58),
    TEXOFFS(TEX_MARIO_BIG_DUCK, "mario/big/duck", 15, 42),
    TEXOFFS(TEX_MARIO_BIG_SWIM1, "mario/big/swim1", 22, 53),
    TEXOFFS(TEX_MARIO_BIG_SWIM2, "mario/big/swim2", 19, 53),
    TEXOFFS(TEX_MARIO_BIG_SWIM3, "mario/big/swim3", 21, 58),
    TEXOFFS(TEX_MARIO_BIG_SWIM4, "mario/big/swim4", 22, 53),

    TEXOFFS(TEX_MARIO_FIRE_GROW1, "mario/fire/grow1", 16, 56),
    TEXOFFS(TEX_MARIO_FIRE_GROW2, "mario/fire/grow2", 16, 58),
    TEXOFFS(TEX_MARIO_FIRE, "mario/fire/idle", 15, 58),
    TEXOFFS(TEX_MARIO_FIRE_WALK1, "mario/fire/walk1", 16, 56),
    TEXOFFS(TEX_MARIO_FIRE_WALK2, "mario/fire/walk2", 16, 58),
    TEXOFFS(TEX_MARIO_FIRE_JUMP, "mario/fire/jump", 16, 58),
    TEXOFFS(TEX_MARIO_FIRE_DUCK, "mario/fire/duck", 15, 42),
    TEXOFFS(TEX_MARIO_FIRE_FIRE, "mario/fire/fire", 16, 58),
    TEXOFFS(TEX_MARIO_FIRE_SWIM1, "mario/fire/swim1", 22, 53),
    TEXOFFS(TEX_MARIO_FIRE_SWIM2, "mario/fire/swim2", 19, 53),
    TEXOFFS(TEX_MARIO_FIRE_SWIM3, "mario/fire/swim3", 21, 58),
    TEXOFFS(TEX_MARIO_FIRE_SWIM4, "mario/fire/swim4", 22, 53),

    TEXOFFS(TEX_MARIO_BEETROOT, "mario/beetroot/idle", 15, 61),
    TEXOFFS(TEX_MARIO_BEETROOT_WALK1, "mario/beetroot/walk1", 16, 59),
    TEXOFFS(TEX_MARIO_BEETROOT_WALK2, "mario/beetroot/walk2", 16, 61),
    TEXOFFS(TEX_MARIO_BEETROOT_JUMP, "mario/beetroot/jump", 16, 59),
    TEXOFFS(TEX_MARIO_BEETROOT_DUCK, "mario/beetroot/duck", 15, 45),
    TEXOFFS(TEX_MARIO_BEETROOT_FIRE, "mario/beetroot/fire", 16, 61),
    TEXOFFS(TEX_MARIO_BEETROOT_SWIM1, "mario/beetroot/swim1", 22, 56),
    TEXOFFS(TEX_MARIO_BEETROOT_SWIM2, "mario/beetroot/swim2", 19, 56),
    TEXOFFS(TEX_MARIO_BEETROOT_SWIM3, "mario/beetroot/swim3", 21, 54),
    TEXOFFS(TEX_MARIO_BEETROOT_SWIM4, "mario/beetroot/swim4", 22, 56),

    TEXOFFS(TEX_MARIO_LUI, "mario/lui/idle", 15, 58),
    TEXOFFS(TEX_MARIO_LUI_WALK1, "mario/lui/walk1", 16, 56),
    TEXOFFS(TEX_MARIO_LUI_WALK2, "mario/lui/walk2", 16, 58),
    TEXOFFS(TEX_MARIO_LUI_JUMP, "mario/lui/jump", 16, 58),
    TEXOFFS(TEX_MARIO_LUI_DUCK, "mario/lui/duck", 15, 42),
    TEXOFFS(TEX_MARIO_LUI_SWIM1, "mario/lui/swim1", 22, 53),
    TEXOFFS(TEX_MARIO_LUI_SWIM2, "mario/lui/swim2", 19, 53),
    TEXOFFS(TEX_MARIO_LUI_SWIM3, "mario/lui/swim3", 21, 58),
    TEXOFFS(TEX_MARIO_LUI_SWIM4, "mario/lui/swim4", 22, 53),

    TEXOFFS(TEX_MARIO_HAMMER, "mario/hammer/idle", 17, 60),
    TEXOFFS(TEX_MARIO_HAMMER_WALK1, "mario/hammer/walk1", 16, 58),
    TEXOFFS(TEX_MARIO_HAMMER_WALK2, "mario/hammer/walk2", 17, 60),
    TEXOFFS(TEX_MARIO_HAMMER_JUMP, "mario/hammer/jump", 16, 58),
    TEXOFFS(TEX_MARIO_HAMMER_DUCK, "mario/hammer/duck", 15, 44),
    TEXOFFS(TEX_MARIO_HAMMER_FIRE, "mario/hammer/fire", 16, 60),
    TEXOFFS(TEX_MARIO_HAMMER_SWIM1, "mario/hammer/swim1", 22, 55),
    TEXOFFS(TEX_MARIO_HAMMER_SWIM2, "mario/hammer/swim2", 19, 55),
    TEXOFFS(TEX_MARIO_HAMMER_SWIM3, "mario/hammer/swim3", 21, 55),
    TEXOFFS(TEX_MARIO_HAMMER_SWIM4, "mario/hammer/swim4", 22, 55),

    TEXOFFS(TEX_COIN1, "items/coin1", -6, -2),
    TEXOFFS(TEX_COIN2, "items/coin2", -6, -2),
    TEXOFFS(TEX_COIN3, "items/coin3", -6, -2),

    TEXOFFS(TEX_MUSHROOM, "items/mushroom", 15, 32),
    TEXOFFS(TEX_MUSHROOM_1UP, "items/mushroom_1up", 15, 32),
    TEXOFFS(TEX_MUSHROOM_POISON1, "items/mushroom_poison1", 15, 32),
    TEXOFFS(TEX_MUSHROOM_POISON2, "items/mushroom_poison2", 15, 32),

    TEXOFFS(TEX_FIRE_FLOWER1, "items/fire_flower1", 16, 31),
    TEXOFFS(TEX_FIRE_FLOWER2, "items/fire_flower2", 16, 31),
    TEXOFFS(TEX_FIRE_FLOWER3, "items/fire_flower3", 16, 31),
    TEXOFFS(TEX_FIRE_FLOWER4, "items/fire_flower4", 16, 31),

    TEXOFFS(TEX_BEETROOT1, "items/beetroot1", 13, 32),
    TEXOFFS(TEX_BEETROOT2, "items/beetroot2", 13, 31),
    TEXOFFS(TEX_BEETROOT3, "items/beetroot3", 13, 30),

    TEXOFFS(TEX_LUI1, "items/lui1", 15, 30),
    TEXOFFS(TEX_LUI2, "items/lui2", 15, 30),
    TEXOFFS(TEX_LUI3, "items/lui3", 15, 30),
    TEXOFFS(TEX_LUI4, "items/lui4", 15, 30),
    TEXOFFS(TEX_LUI5, "items/lui5", 15, 30),
    TEXOFFS(TEX_LUI_BOUNCE1, "items/lui_bounce1", 15, 28),
    TEXOFFS(TEX_LUI_BOUNCE2, "items/lui_bounce2", 15, 26),
    TEXOFFS(TEX_LUI_BOUNCE3, "items/lui_bounce3", 15, 25),

    TEXOFFS(TEX_HAMMER_SUIT, "items/hammer_suit", 13, 31),

    TEXOFFS(TEX_EXPLODE1, "effects/explode1", 8, 7),
    TEXOFFS(TEX_EXPLODE2, "effects/explode2", 12, 13),
    TEXOFFS(TEX_EXPLODE3, "effects/explode3", 16, 15),

    TEXOFFS(TEX_MISSILE_FIREBALL, "missiles/fireball", 7, 7),
    TEXOFFS(TEX_MISSILE_BEETROOT, "missiles/beetroot", 11, 31),
    TEXOFFS(TEX_MISSILE_HAMMER, "missiles/hammer", 13, 18),
    TEXOFFS(TEX_MISSILE_SILVER_HAMMER, "missiles/silver_hammer", 13, 18),
    TEXOFFS(TEX_MISSILE_SPIKE_BALL, "missiles/spike_ball", 20, 20),

    TEXOFFS(TEX_100, "hud/100", 12, 16),
    TEXOFFS(TEX_200, "hud/200", 13, 16),
    TEXOFFS(TEX_500, "hud/500", 13, 16),
    TEXOFFS(TEX_1000, "hud/1000", 16, 16),
    TEXOFFS(TEX_2000, "hud/2000", 17, 16),
    TEXOFFS(TEX_5000, "hud/5000", 17, 16),
    TEXOFFS(TEX_10000, "hud/10000", 20, 16),
    TEXOFFS(TEX_1000000, "hud/1000000", 51, 16),
    TEXOFFS(TEX_1UP, "hud/1up", 16, 16),

    TEXTURE(TEX_ITEM_BLOCK1, "items/block1"),
    TEXTURE(TEX_ITEM_BLOCK2, "items/block2"),
    TEXTURE(TEX_ITEM_BLOCK3, "items/block3"),
    TEXTURE(TEX_EMPTY_BLOCK, "items/empty_block"),
    TEXTURE(TEX_BRICK_BLOCK, "items/brick"),
    TEXTURE(TEX_BRICK_BLOCK_GRAY, "items/brick_gray"),
    TEXOFFS(TEX_BRICK_SHARD, "effects/shard", 8, 8),
    TEXOFFS(TEX_BRICK_SHARD_GRAY, "effects/shard_gray", 8, 8),
};

static vec2 camera = {HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT};
static mat4 mvp = GLM_MAT4_IDENTITY_INIT;
static struct VertexBatch batch = {0};

static struct TileBatch* tiles[TEX_SIZE] = {NULL};
static struct TileBatch* valid_tiles = NULL;

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
                                           "       sample.a = 1.0;\n"
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

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uniforms.texture, 0);
}

void video_update() {
    draw_time = SDL_GetTicks();

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm_ortho(
        camera[0] - HALF_SCREEN_WIDTH, camera[0] + HALF_SCREEN_WIDTH, camera[1] + HALF_SCREEN_HEIGHT,
        camera[1] - HALF_SCREEN_HEIGHT, -16000, 16000, mvp
    );
    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)mvp);

    struct TileBatch* tilemap = valid_tiles;
    if (tilemap != NULL) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
        glUniform1f(uniforms.alpha_test, 0.5f);
        while (tilemap != NULL) {
            glBindVertexArray(tilemap->vao);
            glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(struct Vertex) * tilemap->vertex_count), tilemap->vertices
            );
            glBindTexture(GL_TEXTURE_2D, tilemap->texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tilemap->vertex_count);
            tilemap = tilemap->next;
        }
    }
    draw_state();
    submit_batch();
    SDL_GL_SwapWindow(window);
}

void video_teardown() {
    glDeleteVertexArrays(1, &(batch.vao));
    glDeleteBuffers(1, &(batch.vbo));
    clear_tiles();
    for (size_t i = 0; i < TEX_SIZE; i++)
        glDeleteTextures(1, &(textures[i].texture));
    glDeleteProgram(shader);
    SDL_free(batch.vertices);

    SDL_GL_DestroyContext(gpu);
    SDL_DestroyWindow(window);
}

void move_camera(float x, float y) {
    camera[0] = x;
    camera[1] = y;
    move_ears(x, y);
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

    const char* file = find_file(texture->name);
    if (file == NULL)
        FATAL("Texture \"%s\" not found", texture->name);
    SDL_Surface* surface = IMG_Load(file);
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

    glBindVertexArray(batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(struct Vertex) * batch.vertex_count), batch.vertices);

    // Apply texture
    glBindTexture(GL_TEXTURE_2D, batch.texture);
    const GLint filter = batch.filter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glUniform1f(uniforms.alpha_test, batch.alpha_test);

    // Apply blend mode
    glBlendFuncSeparate(batch.blend_src[0], batch.blend_dest[0], batch.blend_src[1], batch.blend_dest[1]);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
    batch.vertex_count = 0;
}

void clear_tiles() {
    for (size_t i = 0; i < TEX_SIZE; i++) {
        struct TileBatch* tilemap = tiles[i];
        if (tilemap != NULL) {
            glDeleteVertexArrays(1, &(tilemap->vao));
            glDeleteBuffers(1, &(tilemap->vbo));
            SDL_free(tilemap->vertices);
            SDL_free(tilemap);
            tiles[i] = NULL;
        }
    }
}

static struct TileBatch* load_tile(enum TextureIndices index) {
    load_texture(index);
    struct Texture* texture = &(textures[index]);

    struct TileBatch* tilemap = tiles[index];
    if (tilemap == NULL) {
        tilemap = tiles[index] = SDL_malloc(sizeof(struct TileBatch));
        if (tilemap == NULL)
            FATAL("Out of memory for tile batch %u", index);

        tilemap->next = valid_tiles;
        tilemap->texture = texture->texture;
        glGenVertexArrays(1, &(tilemap->vao));
        glBindVertexArray(tilemap->vao);
        glEnableVertexArrayAttrib(tilemap->vao, VATT_POSITION);
        glVertexArrayAttribFormat(tilemap->vao, VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3);
        glEnableVertexArrayAttrib(tilemap->vao, VATT_COLOR);
        glVertexArrayAttribFormat(tilemap->vao, VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLubyte) * 4);
        glEnableVertexArrayAttrib(tilemap->vao, VATT_UV);
        glVertexArrayAttribFormat(tilemap->vao, VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);

        tilemap->vertex_count = 0;
        tilemap->vertex_capacity = 6;
        tilemap->vertices = SDL_malloc(tilemap->vertex_capacity * sizeof(struct Vertex));
        if (tilemap->vertices == NULL)
            FATAL("Out of memory for tile batch %u vertices", index);

        glGenBuffers(1, &(tilemap->vbo));
        glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
        glBufferData(
            GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(struct Vertex) * tilemap->vertex_capacity), NULL, GL_STATIC_DRAW
        );

        glEnableVertexAttribArray(VATT_POSITION);
        glVertexAttribPointer(
            VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, position)
        );

        glEnableVertexAttribArray(VATT_COLOR);
        glVertexAttribPointer(
            VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, color)
        );

        glEnableVertexAttribArray(VATT_UV);
        glVertexAttribPointer(
            VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, uv)
        );

        valid_tiles = tilemap;
    }

    return tilemap;
}

static void tile_vertex(
    struct TileBatch* tilemap, GLfloat x, GLfloat y, GLfloat z, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat u,
    GLfloat v
) {
    if (tilemap->vertex_count >= tilemap->vertex_capacity) {
        const size_t new_size = tilemap->vertex_capacity * 2;
        if (new_size < tilemap->vertex_capacity)
            FATAL("Capacity overflow in tile batch %p", tilemap);
        tilemap->vertices = SDL_realloc(tilemap->vertices, new_size * sizeof(struct Vertex));
        if (tilemap->vertices == NULL)
            FATAL("Out of memory for tile batch %p vertices", tilemap);

        tilemap->vertex_capacity = new_size;

        glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
        glBufferData(
            GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(struct Vertex) * tilemap->vertex_capacity), NULL, GL_STATIC_DRAW
        );
    }

    tilemap->vertices[tilemap->vertex_count++] = (struct Vertex){x, y, z, r, g, b, a, u, v};
}

void add_gradient(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, GLfloat z, GLubyte colors[4][4]) {
    struct TileBatch* tilemap = load_tile(TEX_NULL);
    tile_vertex(tilemap, x1, y2, z, colors[2][0], colors[2][1], colors[2][2], colors[2][3], 0, 1);
    tile_vertex(tilemap, x1, y1, z, colors[0][0], colors[0][1], colors[0][2], colors[0][3], 0, 0);
    tile_vertex(tilemap, x2, y1, z, colors[1][0], colors[1][1], colors[1][2], colors[1][3], 1, 0);
    tile_vertex(tilemap, x2, y1, z, colors[1][0], colors[1][1], colors[1][2], colors[1][3], 1, 0);
    tile_vertex(tilemap, x2, y2, z, colors[3][0], colors[3][1], colors[3][2], colors[3][3], 1, 1);
    tile_vertex(tilemap, x1, y2, z, colors[2][0], colors[2][1], colors[2][2], colors[2][3], 0, 1);
}

void add_backdrop(
    enum TextureIndices index, GLfloat x, GLfloat y, GLfloat z, GLubyte r, GLubyte g, GLubyte b, GLubyte a
) {
    struct TileBatch* tilemap = load_tile(index);
    struct Texture* texture = &(textures[index]);

    const GLfloat x1 = x - texture->offset[0];
    const GLfloat y1 = y - texture->offset[1];
    const GLfloat x2 = x1 + (GLfloat)texture->size[0];
    const GLfloat y2 = y1 + (GLfloat)texture->size[1];

    tile_vertex(tilemap, x1, y2, z, r, g, b, a, 0, 1);
    tile_vertex(tilemap, x1, y1, z, r, g, b, a, 0, 0);
    tile_vertex(tilemap, x2, y1, z, r, g, b, a, 1, 0);
    tile_vertex(tilemap, x2, y1, z, r, g, b, a, 1, 0);
    tile_vertex(tilemap, x2, y2, z, r, g, b, a, 1, 1);
    tile_vertex(tilemap, x1, y2, z, r, g, b, a, 0, 1);
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

void draw_sprite(
    enum TextureIndices index, const GLfloat pos[3], const bool flip[2], GLfloat angle, const GLubyte color[4]
) {
    struct Texture* texture = &(textures[index]);
    GLuint tex = texture->texture;
    if (tex == 0)
        FATAL("Invalid texture index %u", index);
    if (batch.texture != tex)
        submit_batch();
    batch.texture = tex;

    const GLfloat x1 = -(flip[0] ? ((GLfloat)(texture->size[0]) - (texture->offset[0])) : (texture->offset[0]));
    const GLfloat y1 = -(flip[1] ? ((GLfloat)(texture->size[1]) - (texture->offset[1])) : (texture->offset[1]));
    const GLfloat x2 = x1 + (GLfloat)texture->size[0];
    const GLfloat y2 = y1 + (GLfloat)texture->size[1];
    const GLfloat z = pos[2];

    vec2 p1 = {x1, y1};
    vec2 p2 = {x2, y1};
    vec2 p3 = {x1, y2};
    vec2 p4 = {x2, y2};
    if (angle != 0) {
        const float rad = glm_rad(angle);
        glm_vec2_rotate(p1, rad, p1);
        glm_vec2_rotate(p2, rad, p2);
        glm_vec2_rotate(p3, rad, p3);
        glm_vec2_rotate(p4, rad, p4);
    }
    glm_vec2_add((float*)pos, p1, p1);
    glm_vec2_add((float*)pos, p2, p2);
    glm_vec2_add((float*)pos, p3, p3);
    glm_vec2_add((float*)pos, p4, p4);

    const GLfloat u1 = flip[0];
    const GLfloat v1 = flip[1];
    const GLfloat u2 = (GLfloat)(!flip[0]);
    const GLfloat v2 = (GLfloat)(!flip[1]);

    batch_vertex(p3[0], p3[1], z, color[0], color[1], color[2], color[3], u1, v2);
    batch_vertex(p1[0], p1[1], z, color[0], color[1], color[2], color[3], u1, v1);
    batch_vertex(p2[0], p2[1], z, color[0], color[1], color[2], color[3], u2, v1);
    batch_vertex(p2[0], p2[1], z, color[0], color[1], color[2], color[3], u2, v1);
    batch_vertex(p4[0], p4[1], z, color[0], color[1], color[2], color[3], u2, v2);
    batch_vertex(p3[0], p3[1], z, color[0], color[1], color[2], color[3], u1, v2);
}
