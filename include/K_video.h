#pragma once

#include <limits.h>
// ^ required for `CHAR_MAX` below. DO TOUCH YOU LOVER

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
#define XY(x, y) XYZ(x, y, 0)
#define ORIGO XY(0, 0)

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
	GLfloat width;
	GLfloat uvs[4];
} Glyph;

typedef struct {
	char* name;
	const Texture* texture;

	GLfloat height;
	GLfloat spacing;
	Glyph glyphs[CHAR_MAX + 1];
} Font;

typedef struct {
	GLuint vao, vbo;
	size_t vertex_count, vertex_capacity;
	Vertex* vertices;

	GLfloat cursor[3], angle;
	GLubyte color[4][4];
	int8_t halign : 4, valign : 4;
	GLfloat tint[4], stencil;
	GLuint texture;
	GLfloat alpha_test;
	GLenum blend_src[2], blend_dest[2];
	bool filter;
} VertexBatch;

typedef struct TileMap {
	struct TileMap* next;
	const Texture* texture;
	bool translucent;

	GLuint vao, vbo;
	size_t vertex_count, vertex_capacity;
	Vertex* vertices;
} TileMap;

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

// State
void start_video_state();
void tick_video_state();
void save_video_state(VideoState*);
void load_video_state(const VideoState*);
void nuke_video_state();

// Display
void get_resolution(int*, int*);
void set_resolution(int, int);
bool get_fullscreen();
void set_fullscreen(bool);
bool get_vsync();
void set_vsync(bool);

bool window_maximized();
bool window_focused();
bool window_start_typing();
void window_stop_typing();

// Basic
void clear_color(GLfloat, GLfloat, GLfloat, GLfloat);
void clear_depth(GLfloat);

// Textures
void load_texture(const char*);
const Texture* get_texture(const char*);

// Fonts
typedef enum {
	FA_LEFT = -1,
	FA_CENTER = 0,
	FA_RIGHT = 1,

	FA_TOP = -1,
	FA_MIDDLE = 0,
	FA_BOTTOM = 1,
} FontAlignment;

void load_font(const char*);
const Font* get_font(const char*);

// Painting tools
void batch_cursor(const GLfloat[3]), batch_angle(const GLfloat);
void batch_color(const GLubyte[4]), batch_colors(const GLubyte[4][4]);
void batch_align(const FontAlignment, const FontAlignment);
void batch_start(const GLfloat[3], const GLfloat, const GLubyte[4]);

// Batch
void batch_filter(bool);
void batch_alpha_test(GLfloat);
void batch_stencil(GLfloat);
void batch_blendmode(GLenum, GLenum, GLenum, GLenum);

void batch_rectangle(const char*, const GLfloat[2]);
void batch_sprite(const char*, const GLboolean[2]);
void batch_surface(Surface*);
void batch_ellipse(const GLfloat[2]);
GLfloat string_width(const char*, GLfloat, const char*);
GLfloat string_height(const char*, GLfloat, const char*);
void batch_string(const char*, GLfloat, const char*);
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

// Tiles
void tile_sprite(const char*, const GLfloat[3], const GLfloat[2], const GLubyte[4]);
void tile_rectangle(const char*, const GLfloat[2][2], GLfloat, const GLubyte[4][4]);
void draw_tilemaps();
void clear_tilemaps();
