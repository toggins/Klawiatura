#pragma once

#include "K_assets.h"
#include "K_file.h" // IWYU pragma: export
#include "K_math.h"
#include "K_misc.h"
#include "K_vmath.h" // IWYU pragma: export

// Shortcut macros for graphic functions
#define B_F2(x, y) ((float[2]){(x), (y)})
#define B_F3(x, y, z) ((float[3]){(x), (y), (z)})
#define B_F4(x, y, z, w) ((float[4]){(x), (y), (z), (w)})

#define B_B2(x, y) ((Bool[2]){(x), (y)})

#define B_U4(x, y, z, w) ((Uint8[4]){(x), (y), (z), (w)})
#define B_U4X4(...) ((Uint8[4][4]){__VA_ARGS__})

#define B_F2_0 B_F2(0.f, 0.f)
#define B_F2_1 B_F2(1.f, 1.f)
#define B_F2_S(x) B_F2((x), (x))
#define B_F2_SCREEN B_F2(SCREEN_WIDTH, SCREEN_HEIGHT)

#define B_F3_0 B_F3(0.f, 0.f, 0.f)
#define B_F3_1 B_F3(1.f, 1.f, 1.f)
#define B_F3_XY(x, y) B_F3(x, y, 0.f)
#define B_F3_SCREEN B_F3(SCREEN_WIDTH, SCREEN_HEIGHT, 0.f)
#define B_F3_HALF_SCREEN B_F3(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0.f)

#define B_F4_0 B_F4(0.f, 0.f, 0.f, 0.f)
#define B_F4_1 B_F4(1.f, 1.f, 1.f, 1.f)
#define B_F4_RGB(r, g, b) B_F4(r, g, b, 1.f)
#define B_F4_VALUE(v) B_F4_RGB(v, v, v)
#define B_F4_ALPHA(a) B_F4(1.f, 1.f, 1.f, a)

#define B_B2_FALSE B_B2(FALSE, FALSE)
#define B_B2_TRUE B_B2(TRUE, TRUE)

#define B_U4_RGB(r, g, b) B_U4(r, g, b, 255)
#define B_U4_VALUE(v) B_U4_RGB(v, v, v)
#define B_U4_ALPHA(a) B_U4(255, 255, 255, a)
#define B_U4_WHITE B_U4_VALUE(255)
#define B_U4_GRAY B_U4_VALUE(128)
#define B_U4_BLACK B_U4_VALUE(0)
#define B_U4_RED B_U4_RGB(255, 0, 0)
#define B_U4_YELLOW B_U4_RGB(255, 255, 0)
#define B_U4_GREEN B_U4_RGB(0, 255, 0)
#define B_U4_BLUE B_U4_RGB(0, 0, 255)

#define B_U4X4_WHITE B_U4X4({255, 247, 247, 255}, {239, 239, 239, 255}, {222, 214, 206, 255}, {189, 189, 189, 255})
#define B_U4X4_GRAY B_U4X4({160, 160, 160, 255}, {120, 120, 120, 255}, {111, 107, 103, 255}, {95, 95, 95, 255})
#define B_U4X4_RED B_U4X4({255, 189, 165, 255}, {255, 173, 148, 255}, {231, 90, 66, 255}, {181, 41, 16, 255})
#define B_U4X4_YELLOW B_U4X4({255, 247, 57, 255}, {239, 247, 16, 255}, {198, 222, 0, 255}, {132, 140, 0, 255})
#define B_U4X4_GREEN B_U4X4({255, 255, 255, 255}, {181, 247, 181, 255}, {156, 206, 156, 255}, {33, 90, 33, 255})
#define B_U4X4_BLUE B_U4X4({231, 247, 247, 255}, {206, 222, 247, 255}, {132, 148, 206, 255}, {82, 115, 165, 255})
#define B_U4X4_PINK B_U4X4({255, 247, 247, 255}, {255, 222, 239, 255}, {222, 165, 181, 255}, {181, 115, 132, 255})

#define B_ALIGN(h, v) ((FontAlignment[2]){h, v})
#define B_ALIGN_TOP_LEFT B_ALIGN(FA_LEFT, FA_TOP)
#define B_ALIGN_TOP_RIGHT B_ALIGN(FA_RIGHT, FA_TOP)
#define B_ALIGN_CENTER B_ALIGN(FA_CENTER, FA_MIDDLE)
#define B_ALIGN_BOTTOM_LEFT B_ALIGN(FA_LEFT, FA_BOTTOM)
#define B_ALIGN_BOTTOM_RIGHT B_ALIGN(FA_RIGHT, FA_BOTTOM)

typedef Uint8 VertexAttribute;
enum {
    VATT_POSITION,
    VATT_COLOR,
    VATT_UV,
};

typedef struct {
    float position[3];
    Uint8 color[4];
    float uv[2];
} Vertex;

typedef Uint8 ShaderType;
enum {
    SH_MAIN,
    SH_SIZE,
};

typedef Uint8 UniformType;
enum {
    UNI_MVP,
    UNI_TEXTURE,
    UNI_ALPHA_TEST,
    UNI_STENCIL,
    UNI_SIZE,
};

typedef struct {
    AssetBase base;

    Uint16 size[2];

    void* internal;
} Texture;

typedef struct {
    AssetBase base;

    TinyHash texture_key;
    float size[2], offset[2], uvs[4];
} Sprite;

typedef struct {
    TinyHash texture_key;
    float bounds[4], uvs[4], advance;
} Glyph;

typedef struct {
    AssetBase base;
    TinyMap glyphs;
} Font;

typedef Sint8 FontAlignment;
enum {
    FA_LEFT = -1,
    FA_CENTER = 0,
    FA_RIGHT = 1,

    FA_TOP = -1,
    FA_MIDDLE = 0,
    FA_BOTTOM = 1,
};

typedef Uint8 SurfaceAttribute;
enum {
    SURF_COLOR,
    SURF_DEPTH,
    SURF_SIZE,
};

typedef struct Surface {
    Bool active;
    struct Surface* previous;

    mat4 model_matrix, view_matrix, projection_matrix, mvp_matrix;

    Bool enabled[SURF_SIZE];
    Uint16 size[2];

    void* internal;
} Surface;

typedef Uint8 BlendMode;
enum {
    BM_NORMAL,
    BM_ADD,
    BM_SUBTRACT,
    BM_MULTIPLY,
};

typedef Uint8 StencilFunction;
enum {
    STF_NEVER,
    STF_LESS,
    STF_LESS_OR_EQUAL,
    STF_EQUAL,
    STF_GREATER_OR_EQUAL,
    STF_GREATER,
    STF_ALWAYS,
};

typedef Uint8 StencilOperation;
enum {
    STO_KEEP,
    STO_ZERO,
    STO_REPLACE,
    STO_INCREMENT,
    STO_INCREMENT_WRAP,
    STO_DECREMENT,
    STO_DECREMENT_WRAP,
    STO_INVERT,
};

typedef struct TileMapLayer {
    TinyPriority depth;

    void* internal;
} TileMapLayer;

typedef struct {
    void* internal;
} TileMap;

typedef struct {
    FVec2 pos, from;
    float lerp_time, lerp_duration;
} VideoCamera;

typedef struct {
    Uint8 hurry;
    VideoCamera camera;
    TileMap* tilemap;
} VideoState;

void video_init(Bool), video_teardown();
void start_drawing(), start_drawing_ui(), stop_drawing();
void limit_framerate();

// Display
void get_resolution(int*, int*), set_resolution(int, int, Bool);
Bool get_fullscreen();
void set_fullscreen(Bool);
int get_framerate();
void set_framerate(int);
Bool get_vsync();
void set_vsync(Bool);

Bool window_maximized(), window_focused();

// Basic
void clear_color(const float[4]), clear_depth(float), clear_stencil(Uint8);

// Shaders
void set_shader(ShaderType);

void set_int_uniform(UniformType, int);
void set_float_uniform(UniformType, float);
void set_vec2_uniform(UniformType, const float[2]);
void set_vec3_uniform(UniformType, const float[3]);
void set_vec4_uniform(UniformType, const float[4]);
void set_mat4_uniform(UniformType, const float[4][4]);

// Assets
ASSET_HEAD(textures, Texture, texture);
ASSET_HEAD(sprites, Sprite, sprite);
ASSET_HEAD(fonts, Font, font);

// Batch
void batch_pos(const float[3]), batch_offset(const float[3]), batch_scale(const float[2]), batch_angle(float);
void batch_color(const Uint8[4]), batch_colors(const Uint8[4][4]), batch_stencil(const float[4]);
void batch_flip(const Bool[2]), batch_tile(const Bool[2]), batch_align(const FontAlignment[2]);
void batch_filter(Bool), batch_alpha_test(float);
void batch_blend(BlendMode);
void batch_write_color(Bool, Bool, Bool, Bool);
void batch_test_depth(Bool), batch_write_depth(Bool);
void batch_test_stencil(Bool), batch_stencil_mask(Uint8), batch_stencil_func(StencilFunction, Uint8, Uint8),
    batch_stencil_op(StencilOperation, StencilOperation, StencilOperation);
void batch_reset(), batch_reset_hard();

void batch_rectangle(const char*, const float[2]), batch_circle(const char*, float);
void batch_sprite(const char*), batch_surface(const Surface*);
float string_width(const char*, float, const char*), string_height(const char*, float, const char*);
void batch_string(const char*, float, const char*);
float string_width_wrap(const char*, float, const char*, float),
    string_height_wrap(const char*, float, const char*, float);
void batch_string_wrap(const char*, float, const char*, float);

void batch_primitive(const char*), batch_primitive_direct(const Texture*), batch_primitive_surface(Surface*);
void batch_vertex(const float[3], const Uint8[4], const float[2]);

void submit_batch();

// Matrices
void get_model_matrix(mat4), get_view_matrix(mat4), get_projection_matrix(mat4);
const mat4* get_mvp_matrix();

void set_model_matrix(mat4), set_view_matrix(mat4), set_projection_matrix(mat4);
void apply_matrices();

// Surfaces
Surface* create_surface(Uint16, Uint16, Bool, Bool);
void destroy_surface(Surface*);
void check_surface(Surface*), dispose_surface(Surface*);
void resize_surface(Surface*, Uint16, Uint16);
void push_surface(Surface*), pop_surface();

// Tilemaps
TileMap* create_tilemap();
void destroy_tilemap(TileMap*);

void
add_tilemap(TileMap*, const char*, const float[3], const float[2], const Bool[2], const Bool[2], const Uint8[4][4]);
void read_tilemap(TileMap*, yyjson_val*);

void tilemap_iterate_start(TileMap*);
const TileMapLayer* tilemap_iterate_next(TileMap*);
void draw_tilemap_layer(const TileMapLayer*), draw_tilemap(TileMap*);

// State
void start_video_state(), tick_video_state(), nuke_video_state();
VideoState* videostate();
