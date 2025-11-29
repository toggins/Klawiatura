#ifndef K_VIDEO_H
#define K_VIDEO_H

#include <glad/gl.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>

#include <limits.h> // required for `CHAR_MAX`

#define CGLM_ALL_UNALIGNED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include <cglm/cglm.h> // IWYU pragma: keep

#include "K_game.h"

// Shortcut macros for graphic functions
#define B_XYZ(x, y, z) ((GLfloat[3]){(x), (y), (z)})
#define B_XY(x, y) B_XYZ(x, y, 0)
#define B_ORIGIN B_XY(0, 0)
#define B_SCREEN B_XY(SCREEN_WIDTH, SCREEN_HEIGHT)
#define B_SCALE(x) B_XYZ(x, x, x)

#define B_RGBA(r, g, b, a) ((GLubyte[4]){(r), (g), (b), (a)})
#define B_RGB(r, g, b) B_RGBA(r, g, b, 255)
#define B_VALUE(v) B_RGB(v, v, v)
#define B_ALPHA(a) B_RGBA(255, 255, 255, a)

#define B_WHITE B_VALUE(255)
#define B_GRAY B_VALUE(128)
#define B_BLACK B_VALUE(0)
#define B_RED B_RGB(255, 0, 0)
#define B_GREEN B_RGB(0, 255, 0)
#define B_BLUE B_RGB(0, 0, 255)

#define B_STENCIL(r, g, b, v) ((GLfloat[4]){r, g, b, v})
#define B_NO_STENCIL B_STENCIL(0, 0, 0, 0)

#define B_UV(u, v) ((GLfloat[2]){(u), (v)})

#define B_FLIP(x, y) ((GLboolean[2]){(x), (y)})
#define B_NO_FLIP B_FLIP(false, false)

#define B_TILE(x, y) ((GLboolean[2]){(x), (y)})
#define B_NO_TILE B_TILE(false, false)

#define B_ALIGN(h, v) ((FontAlignment[2]){h, v})
#define B_TOP_LEFT B_ALIGN(FA_LEFT, FA_TOP)
#define B_TOP_RIGHT B_ALIGN(FA_RIGHT, FA_TOP)
#define B_BOTTOM_LEFT B_ALIGN(FA_LEFT, FA_BOTTOM)
#define B_BOTTOM_RIGHT B_ALIGN(FA_RIGHT, FA_BOTTOM)
#define B_CENTER B_ALIGN(FA_CENTER, FA_MIDDLE)

#define B_BLEND(cs, cd, as, ad)                                                                                        \
	((GLenum[2][2]){                                                                                               \
		{cs, cd}, \
                {as, ad} \
        })
#define B_BLEND_NORMAL B_BLEND(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE)

typedef uint8_t VertexAttribute;
enum {
	VATT_POSITION,
	VATT_COLOR,
	VATT_UV,
};

typedef struct {
	GLfloat position[3];
	GLubyte color[4];
	GLfloat uv[2];
} Vertex;

typedef uint8_t ShaderType;
enum {
	SH_MAIN,
	SH_WAVE,
	SH_SIZE,
};

typedef uint8_t UniformType;
enum {
	UNI_MVP,
	UNI_TEXTURE,
	UNI_ALPHA_TEST,
	UNI_STENCIL,
	UNI_WAVE,
	UNI_TIME,
	UNI_SIZE,
};

typedef struct {
	const char* name;
	GLint shader, uniforms[UNI_SIZE];
} Shader;

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

typedef int8_t FontAlignment;
enum {
	FA_LEFT = -1,
	FA_CENTER = 0,
	FA_RIGHT = 1,

	FA_TOP = -1,
	FA_MIDDLE = 0,
	FA_BOTTOM = 1,
};

typedef struct {
	GLuint vao, vbo;
	size_t vertex_count, vertex_capacity;
	Vertex* vertices;

	GLfloat pos[3], offset[3], scale[3], angle;
	GLubyte color[4][4];
	GLboolean flip[2], tile[2];
	FontAlignment align[2];
	GLfloat stencil[4];
	GLuint texture;
	GLboolean filter;
	GLfloat alpha_test;
	GLenum blend[2][2];
} VertexBatch;

typedef struct TileMap {
	struct TileMap* next;
	const Texture* texture;
	bool translucent;

	GLuint vao, vbo;
	size_t vertex_count, vertex_capacity;
	Vertex* vertices;
} TileMap;

typedef uint8_t SurfaceAttribute;
enum {
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
	vec2 pos, from;
	float lerp_time[2];
} VideoCamera;

typedef struct {
	char world[GAME_STRING_MAX];
	float quake, hurry, boss, message_time;
	const char* message;
	Surface* fred_surface;
	VideoCamera camera;
} VideoState;

extern VideoState video_state;

void video_init(bool), video_teardown();
void start_drawing(), stop_drawing();

// State
void start_video_state(), nuke_video_state();

// Display
void get_resolution(int*, int*), set_resolution(int, int);
bool get_fullscreen();
void set_fullscreen(bool);
bool get_vsync();
void set_vsync(bool);

bool window_maximized(), window_focused();
bool window_start_text_input();
void window_stop_text_input();

// Basic
void clear_color(GLfloat, GLfloat, GLfloat, GLfloat), clear_depth(GLfloat);

// Shaders
void set_shader(ShaderType);

void set_int_uniform(UniformType, GLint);
void set_float_uniform(UniformType, GLfloat);
void set_vec2_uniform(UniformType, const GLfloat[2]);
void set_vec3_uniform(UniformType, const GLfloat[3]);
void set_vec4_uniform(UniformType, const GLfloat[4]);
void set_mat4_uniform(UniformType, const GLfloat[4][4]);

// Textures
void load_texture(const char*), load_texture_num(const char*, uint32_t);
const Texture* get_texture(const char*);

// Fonts
void load_font(const char*);
const Font* get_font(const char*);

// Batch
void batch_pos(const GLfloat[3]), batch_offset(const GLfloat[3]), batch_scale(const GLfloat[2]),
	batch_angle(const GLfloat);
void batch_color(const GLubyte[4]), batch_colors(const GLubyte[4][4]), batch_stencil(const GLfloat[4]);
void batch_flip(const GLboolean[2]), batch_tile(const GLboolean[2]), batch_align(const FontAlignment[2]);
void batch_filter(bool), batch_alpha_test(GLfloat);
void batch_blend(const GLenum[2][2]);
void batch_reset(), batch_reset_hard();

void batch_rectangle(const char*, const GLfloat[2]);
void batch_sprite(const char*), batch_surface(Surface*);
GLfloat string_width(const char*, GLfloat, const char*), string_height(const char*, GLfloat, const char*);
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
void check_surface(Surface*), dispose_surface(Surface*);
void resize_surface(Surface*, GLuint, GLuint);
void push_surface(Surface*), pop_surface();

// Tiles
void tile_sprite(const char*, const GLfloat[3], const GLfloat[2], const GLubyte[4]);
void tile_rectangle(const char*, const GLfloat[2][2], GLfloat, const GLubyte[4][4]);
void draw_tilemaps(), clear_tilemaps();

// Camera
void lerp_camera(float);

#endif
