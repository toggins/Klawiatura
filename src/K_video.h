#pragma once

#include <glad/gl.h>

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>

#define CGLM_ALL_UNALIGNED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include <cglm/cglm.h> // IWYU pragma: keep

#ifdef RGB
#undef RGB
#endif

#define RGBA(r, g, b, a) ((GLubyte[4]){(r), (g), (b), (a)})
#define RGB(r, g, b) RGBA(r, g, b, 255)
#define ALPHA(a) RGBA(255, 255, 255, a)

#define WHITE RGB(255, 255, 255)
#define GRAY RGB(128, 128, 128)
#define BLACK RGB(0, 0, 0)

#define XYZ(x, y, z) ((GLfloat[3]){(x), (y), (z)})

#define FLIP(x, y) ((GLboolean[2]){(x), (y)})
#define NO_FLIP FLIP(false, false)

#define UV(u, v) ((GLfloat[2]){(u), (v)})

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
	char* name;
	GLuint texture;
	GLuint size[2];
	GLfloat offset[2];
} Texture;

typedef struct {
	GLuint vao, vbo;
	size_t vertex_count, vertex_capacity;
	Vertex* vertices;

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
	Vertex* vertices;
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

typedef struct {
	void* temp;
} VideoState;

extern VideoState video_state;

void video_init(bool);
void video_teardown();

void start_drawing();
void stop_drawing();

// Display
void set_resolution(int, int);
void set_fullscreen(bool);
void set_vsync(bool);

// Basic
void clear_color(GLfloat, GLfloat, GLfloat, GLfloat);
void clear_depth(GLfloat);

// Textures
void load_texture(const char*);
const Texture* get_texture(const char*);

// Batch
void set_batch_texture(GLuint);
void set_batch_stencil(GLfloat);
void set_batch_logic(GLenum);
void batch_vertex(const GLfloat[3], const GLubyte[4], const GLfloat[2]);
void batch_sprite(const char*, const GLfloat[3], const GLboolean[2], GLfloat, const GLubyte[4]);
void batch_surface(Surface*, const GLfloat[3], const GLubyte[4]);
void batch_rectangle(const char*, const GLfloat[2][2], GLfloat, const GLubyte[4]);
void batch_ellipse(const GLfloat[2][2], GLfloat, const GLubyte[4]);
void submit_batch();

// Matrices
const mat4* get_mvp_matrix();

void set_model_matrix(mat4);
void set_view_matrix(mat4);
void set_projection_matrix(mat4);
void apply_matrices();

// Surfaces
Surface* create_surface(GLuint, GLuint, bool, bool);
void destroy_surface(Surface*);
void check_surface(Surface*);
void dispose_surface(Surface*);
void resize_surface(Surface*, GLuint, GLuint);
void push_surface(Surface*);
void pop_surface();
