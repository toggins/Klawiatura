#pragma once

// #include order matters!
// clang-format off
#include <glad/gl.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_opengl.h>
// clang-format on

enum VertexAttributes {
    VATT_POSITION,
    VATT_COLOR,
    VATT_UV,
};

enum TextureIndices {
    TEX_NULL,

    // Props
    TEX_CLOUD1,
    TEX_CLOUD2,
    TEX_CLOUD3,
    TEX_BUSH1,
    TEX_BUSH2,
    TEX_BUSH3,

    TEX_PLAYER,

    TEX_BULLET,
    TEX_BULLET_HIT1,
    TEX_BULLET_HIT2,
    TEX_BULLET_HIT3,

    TEX_SIZE,
};

struct Uniforms {
    GLint mvp;
    GLint texture;
    GLint alpha_test;
};

struct Texture {
    const char* name;
    GLuint texture;
    GLuint size[2];
    GLfloat offset[2];
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

    GLfloat color[4];
    GLuint texture;
    GLfloat alpha_test;
    GLenum blend_src[2], blend_dest[2];
    bool filter;
};

void video_init(bool);
void video_update();
void video_teardown();

void load_texture(enum TextureIndices);

void submit_batch();
void batch_vertex(GLfloat, GLfloat, GLfloat, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat);
void draw_sprite(enum TextureIndices, const float[3], const bool[2]);
