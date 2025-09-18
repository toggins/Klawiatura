#pragma once

#include <glad/gl.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_opengl.h>

#define CGLM_ALL_UNALIGNED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include <cglm/cglm.h> // IWYU pragma: keep

#include "K_memory.h" // IWYU pragma: keep

#define RGBA(r, g, b, a)                                                                                               \
    (GLubyte[4]) {                                                                                                     \
        (r), (g), (b), (a)                                                                                             \
    }

#ifdef RGB
#undef RGB
#endif
#define RGB(r, g, b) RGBA(r, g, b, 255)

#define WHITE RGB(255, 255, 255)
#define GRAY RGB(128, 128, 128)
#define BLACK RGB(0, 0, 0)

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

struct Vertex {
    GLfloat position[3];
    GLubyte color[4];
    GLfloat uv[2];
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
    bool translucent;

    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    struct Vertex* vertices;
};

struct Surface {
    bool active;
    struct Surface* previous;

    mat4 model_matrix, view_matrix, projection_matrix, mvp_matrix;

    bool enabled[2];
    GLuint fbo, texture[2];
    GLuint size[2];
};

void video_init(bool);
void video_teardown();

void video_start();
void video_end();
