#ifndef K_VIDEO_H
#define K_VIDEO_H

#include "K_assets.h"
#include "K_vmath.h" // IWYU pragma: export

// Shortcut macros for graphic functions
#define B_XYZ(x, y, z) ((float[3]){(x), (y), (z)})
#define B_XY(x, y) B_XYZ(x, y, 0.f)
#define B_ORIGIN B_XY(0.f, 0.f)
#define B_SCREEN B_XY(SCREEN_WIDTH, SCREEN_HEIGHT)
#define B_HALF_SCREEN B_XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT)

#define B_RGBA(r, g, b, a) ((Uint8[4]){(r), (g), (b), (a)})
#define B_RGB(r, g, b) B_RGBA(r, g, b, 255)
#define B_VALUE(v) B_RGB(v, v, v)
#define B_ALPHA(a) B_RGBA(255, 255, 255, a)

#define B_WHITE B_VALUE(255)
#define B_GRAY B_VALUE(128)
#define B_BLACK B_VALUE(0)
#define B_RED B_RGB(255, 0, 0)
#define B_YELLOW B_RGB(255, 255, 0)
#define B_GREEN B_RGB(0, 255, 0)
#define B_BLUE B_RGB(0, 0, 255)

#define B_MF_WHITE                                                                                                     \
    ((Uint8[4][4]){                                                                                                    \
        {255, 247, 247, 255}, \
        {239, 239, 239, 255}, \
        {222, 214, 206, 255}, \
        {189, 189, 189, 255} \
    })

#define B_MF_GRAY                                                                                                      \
    ((Uint8[4][4]){                                                                                                    \
        {128, 124, 124, 255}, \
        {120, 120, 120, 255}, \
        {111, 107, 103, 255}, \
        {95,  95,  95,  255} \
    })

#define B_MF_RED                                                                                                       \
    ((Uint8[4][4]){                                                                                                    \
        {255, 189, 165, 255}, \
        {255, 173, 148, 255}, \
        {231, 90,  66,  255}, \
        {181, 41,  16,  255} \
    })

#define B_MF_YELLOW                                                                                                    \
    ((Uint8[4][4]){                                                                                                    \
        {255, 247, 57, 255}, \
        {239, 247, 16, 255}, \
        {198, 222, 0,  255}, \
        {132, 140, 0,  255} \
    })

#define B_MF_GREEN                                                                                                     \
    ((Uint8[4][4]){                                                                                                    \
        {255, 255, 255, 255}, \
        {181, 247, 181, 255}, \
        {156, 206, 156, 255}, \
        {33,  90,  33,  255} \
    })

#define B_MF_BLUE                                                                                                      \
    ((Uint8[4][4]){                                                                                                    \
        {231, 247, 247, 255}, \
        {206, 222, 247, 255}, \
        {132, 148, 206, 255}, \
        {82,  115, 165, 255} \
    })

#define B_MF_PINK                                                                                                      \
    ((Uint8[4][4]){                                                                                                    \
        {255, 247, 247, 255}, \
        {255, 222, 239, 255}, \
        {222, 165, 181, 255}, \
        {181, 115, 132, 255} \
    })

#define B_STENCIL(r, g, b, v) ((float[4]){r, g, b, v})
#define B_NO_STENCIL B_STENCIL(0.f, 0.f, 0.f, 0.f)

#define B_UV(u, v) ((float[2]){(u), (v)})

#define B_FLIP(x, y) ((Bool[2]){(x), (y)})
#define B_NO_FLIP B_FLIP(FALSE, FALSE)

#define B_TILE(x, y) ((Bool[2]){(x), (y)})
#define B_NO_TILE B_TILE(FALSE, FALSE)

#define B_ALIGN(h, v) ((FontAlignment[2]){h, v})
#define B_TOP_LEFT B_ALIGN(FA_LEFT, FA_TOP)
#define B_TOP_RIGHT B_ALIGN(FA_RIGHT, FA_TOP)
#define B_BOTTOM_LEFT B_ALIGN(FA_LEFT, FA_BOTTOM)
#define B_BOTTOM_RIGHT B_ALIGN(FA_RIGHT, FA_BOTTOM)
#define B_CENTER B_ALIGN(FA_CENTER, FA_MIDDLE)

#define B_WH(w, h) ((float[2]){(w), (h)})
#define B_SIZE(x) B_WH(x, x)

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
    SH_WAVE,
    SH_SIZE,
};

typedef Uint8 UniformType;
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
void clear_color(float, float, float, float), clear_depth(float), clear_stencil(Uint8);

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
void load_texture_num(const char*, Uint32, Bool);

ASSET_HEAD(sprites, Sprite, sprite);
void load_sprite_num(const char*, Uint32, Bool);

ASSET_HEAD(fonts, Font, font);

// Batch
void batch_pos(const float[3]), batch_offset(const float[3]), batch_scale(const float[2]), batch_angle(float);
void batch_color(const Uint8[4]), batch_colors(const Uint8[4][4]), batch_stencil(const float[4]);
void batch_flip(const Bool[2]), batch_tile(const Bool[2]), batch_align(const FontAlignment[2]);
void batch_filter(Bool), batch_alpha_test(float);
void batch_blend(BlendMode);
void batch_write_color(Bool);
void batch_test_depth(Bool), batch_write_depth(Bool);
void batch_test_stencil(Bool), batch_write_stencil(Bool), batch_stencil_mask(Uint8),
    batch_stencil_func(StencilFunction, Uint8, Uint8),
    batch_stencil_op(StencilOperation, StencilOperation, StencilOperation);
void batch_reset(), batch_reset_hard();

void batch_rectangle(const char*, const float[2]);
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

#endif
