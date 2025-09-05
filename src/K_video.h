#pragma once

#include <glad/gl.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_opengl.h>

#include "K_memory.h" // IWYU pragma: keep

#ifdef RGB
#undef RGB
#endif
#define RGB(r, g, b) (GLubyte[4]){r, g, b, 255}

#define WHITE RGB(255, 255, 255)
#define GRAY RGB(128, 128, 128)
#define BLACK RGB(0, 0, 0, 255)

#define RGBA(r, g, b, a)                                                                                               \
    (GLubyte[4]) {                                                                                                     \
        (r), (g), (b), (a)                                                                                             \
    }

#define ALPHA(a)                                                                                                       \
    (GLubyte[4]) {                                                                                                     \
        255, 255, 255, (a)                                                                                             \
    }

#define XYZ(x, y, z)                                                                                                   \
    (GLfloat[3]) {                                                                                                     \
        (x), (y), (z)                                                                                                  \
    }

enum VertexAttributes {
    VATT_POSITION,
    VATT_COLOR,
    VATT_UV,
};

enum FontIndices {
    FNT_NULL,
    FNT_MAIN,
    FNT_HUD,
    FNT_SIZE,
};

enum FontAlignment {
    FA_LEFT = 0,
    FA_TOP = 0,
    FA_CENTER = 1,
    FA_RIGHT = 2,
    FA_BOTTOM = 2,
};

struct Uniforms {
    GLint mvp;
    GLint texture;
    GLint alpha_test;
    GLint stencil;
};

struct Texture {
    char name[sizeof(StTinyKey)];
    GLuint texture;
    GLuint size[2];
    GLfloat offset[2];
};

struct Font {
    char tname[sizeof(StTinyKey)];
    const struct Texture* texture;

    GLfloat spacing;
    GLfloat line_height;
    struct Glyph {
        GLfloat size[2], uvs[4];
    } glyphs[256];
};

struct Vertex {
    GLfloat position[3];
    GLubyte color[4];
    GLfloat uv[2];
};

struct VertexBatch {
    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    struct Vertex* vertices;

    GLfloat color[4], stencil;
    GLuint texture;
    GLfloat alpha_test;
    GLenum blend_src[2], blend_dest[2];
    GLenum logic;
    bool filter;
};

struct TileBatch {
    struct TileBatch* next;
    const struct Texture* texture;

    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    struct Vertex* vertices;
};

struct VideoState {
    float quake;
};

void video_init(bool);
void video_update(const char*);
void video_teardown();

void save_video_state(struct VideoState*);
void load_video_state(const struct VideoState*);
void tick_video_state();

void quake_at(float[2], float);
float get_quake();
void move_camera(float, float);

void load_texture(const char*);
const struct Texture* get_texture(const char*);

void load_font(enum FontIndices);

void set_batch_stencil(GLfloat);
void set_batch_logic(GLenum);
void submit_batch();
void batch_vertex(GLfloat, GLfloat, GLfloat, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat);
void clear_tiles();
void add_gradient(const char*, const GLfloat[2][2], GLfloat, const GLubyte[4][4]);
void add_backdrop(const char*, const GLfloat[3], const GLfloat[2], const GLubyte[4]);
void draw_sprite(const char*, const float[3], const bool[2], GLfloat, const GLubyte[4]);
GLfloat string_width(enum FontIndices, const char*);
GLfloat string_height(enum FontIndices, const char*);
void draw_text(enum FontIndices, enum FontAlignment, const char*, const float[3]);
void draw_rectangle(const char*, const float[2][2], float, const GLubyte[4]);
void draw_ellipse(const float[2][2], float, const GLubyte[4]);
