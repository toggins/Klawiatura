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

typedef struct {
    GLfloat position[3];
    GLubyte color[4];
    GLfloat uv[2];
} Vertex;

typedef struct {
    GLint mvp;
    GLint texture;
    GLint alpha_test;
    GLint stencil;
} Uniforms;

typedef struct {
    char name[sizeof(StTinyKey)];
    GLuint texture;
    GLuint size[2];
    GLfloat offset[2];
} Texture;

typedef struct {
    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    struct Vertex* vertices;

    GLfloat color[4], stencil;
    GLuint texture;
    GLfloat alpha_test;
    GLenum blend_src[2], blend_dest[2];
    GLenum logic;
    bool filter;
} VertexBatch;

typedef struct TileBatch {
    struct TileBatch* next;
    const struct Texture* texture;
    bool translucent;

    GLuint vao, vbo;
    size_t vertex_count, vertex_capacity;
    struct Vertex* vertices;
} TileBatch;

enum SurfaceAttributes {
    SURF_COLOR,
    SURF_DEPTH,
    SURF_SIZE,
};

typedef struct Surface {
    bool active;
    struct Surface* previous;

    mat4 model_matrix, view_matrix, projection_matrix, mvp_matrix;

    bool enabled[SURF_SIZE];
    GLuint fbo, texture[SURF_SIZE];
    GLuint size[2];
} Surface;

void video_init(bool);
void video_teardown();

void video_start();
void video_end();

Surface* create_surface(GLuint, GLuint, bool, bool);
void destroy_surface(Surface*);
void check_surface(Surface*);
void dispose_surface(Surface*);
void resize_surface(Surface*, GLuint, GLuint);
void push_surface(Surface*);
void pop_surface();
