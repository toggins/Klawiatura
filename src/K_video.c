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
static GLuint blank_texture = 0;

static StTinyMap* textures = NULL;

static struct Font fonts[FNT_SIZE] = {
    [FNT_NULL] = {0},

    [FNT_MAIN] = {
        .tname = "F_MAIN",
        .spacing = -2,
        .line_height = 24,
        .glyphs = {
            [' '] =
                {
                    .size = {8, 24},
                    .uvs = {0},
                },
            ['!'] =
                {
                    .size = {7, 24},
                    .uvs = {1.0f / 256.0f, 1.0f / 256.0f, 8.0f / 256.0f, 25.0f / 256.0f},
                },
            ['\"'] =
                {
                    .size = {11, 24},
                    .uvs = {10.0f / 256.0f, 1.0f / 256.0f, 21.0f / 256.0f, 25.0f / 256.0f},
                },
            ['#'] =
                {
                    .size = {21, 24},
                    .uvs = {23.0f / 256.0f, 1.0f / 256.0f, 8.0f / 256.0f, 25.0f / 256.0f},
                },
            ['$'] =
                {
                    .size = {11, 24},
                    .uvs = {1.0f / 256.0f, 1.0f / 256.0f, 8.0f / 256.0f, 25.0f / 256.0f},
                },
            ['%'] =
                {
                    .size = {18, 24},
                    .uvs = {59.0f / 256.0f, 1.0f / 256.0f, 77.0f / 256.0f, 25.0f / 256.0f},
                },
            ['&'] =
                {
                    .size = {13, 24},
                    .uvs = {79.0f / 256.0f, 1.0f / 256.0f, 92.0f / 256.0f, 25.0f / 256.0f},
                },
            ['\''] =
                {
                    .size = {8, 24},
                    .uvs = {94.0f / 256.0f, 1.0f / 256.0f, 102.0f / 256.0f, 25.0f / 256.0f},
                },
            ['('] =
                {
                    .size = {9, 24},
                    .uvs = {104.0f / 256.0f, 1.0f / 256.0f, 113.0f / 256.0f, 25.0f / 256.0f},
                },
            [')'] =
                {
                    .size = {9, 24},
                    .uvs = {115.0f / 256.0f, 1.0f / 256.0f, 124.0f / 256.0f, 25.0f / 256.0f},
                },
            ['*'] =
                {
                    .size = {11, 24},
                    .uvs = {126.0f / 256.0f, 1.0f / 256.0f, 137.0f / 256.0f, 25.0f / 256.0f},
                },
            ['+'] =
                {
                    .size = {16, 24},
                    .uvs = {139.0f / 256.0f, 1.0f / 256.0f, 155.0f / 256.0f, 25.0f / 256.0f},
                },
            [','] =
                {
                    .size = {9, 24},
                    .uvs = {157.0f / 256.0f, 1.0f / 256.0f, 164.0f / 256.0f, 25.0f / 256.0f},
                },
            ['-'] =
                {
                    .size = {10, 24},
                    .uvs = {168.0f / 256.0f, 1.0f / 256.0f, 178.0f / 256.0f, 25.0f / 256.0f},
                },
            ['.'] =
                {
                    .size = {8, 24},
                    .uvs = {180.0f / 256.0f, 1.0f / 256.0f, 188.0f / 256.0f, 25.0f / 256.0f},
                },
            ['/'] =
                {
                    .size = {11, 24},
                    .uvs = {190.0f / 256.0f, 1.0f / 256.0f, 201.0f / 256.0f, 25.0f / 256.0f},
                },
            ['0'] =
                {
                    .size = {13, 24},
                    .uvs = {203.0f / 256.0f, 1.0f / 256.0f, 216.0f / 256.0f, 25.0f / 256.0f},
                },
            ['1'] =
                {
                    .size = {9, 24},
                    .uvs = {218.0f / 256.0f, 1.0f / 256.0f, 227.0f / 256.0f, 25.0f / 256.0f},
                },
            ['2'] =
                {
                    .size = {12, 24},
                    .uvs = {229.0f / 256.0f, 1.0f / 256.0f, 241.0f / 256.0f, 25.0f / 256.0f},
                },
            ['3'] =
                {
                    .size = {12, 24},
                    .uvs = {243.0f / 256.0f, 1.0f / 256.0f, 255.0f / 256.0f, 25.0f / 256.0f},
                },
            ['4'] =
                {
                    .size = {13, 24},
                    .uvs = {1.0f / 256.0f, 26.0f / 256.0f, 14.0f / 256.0f, 51.0f / 256.0f},
                },
            ['5'] =
                {
                    .size = {11, 24},
                    .uvs = {16.0f / 256.0f, 26.0f / 256.0f, 27.0f / 256.0f, 50.0f / 256.0f},
                },
            ['6'] =
                {
                    .size = {12, 24},
                    .uvs = {29.0f / 256.0f, 26.0f / 256.0f, 41.0f / 256.0f, 50.0f / 256.0f},
                },
            ['7'] =
                {
                    .size = {12, 24},
                    .uvs = {43.0f / 256.0f, 26.0f / 256.0f, 55.0f / 256.0f, 50.0f / 256.0f},
                },
            ['8'] =
                {
                    .size = {13, 24},
                    .uvs = {57.0f / 256.0f, 26.0f / 256.0f, 70.0f / 256.0f, 50.0f / 256.0f},
                },
            ['9'] =
                {
                    .size = {12, 24},
                    .uvs = {72.0f / 256.0f, 26.0f / 256.0f, 84.0f / 256.0f, 50.0f / 256.0f},
                },
            [':'] =
                {
                    .size = {7, 24},
                    .uvs = {86.0f / 256.0f, 26.0f / 256.0f, 93.0f / 256.0f, 50.0f / 256.0f},
                },
            [';'] =
                {
                    .size = {8, 24},
                    .uvs = {95.0f / 256.0f, 26.0f / 256.0f, 103.0f / 256.0f, 50.0f / 256.0f},
                },
            ['<'] =
                {
                    .size = {15, 24},
                    .uvs = {105.0f / 256.0f, 26.0f / 256.0f, 120.0f / 256.0f, 50.0f / 256.0f},
                },
            ['='] =
                {
                    .size = {15, 24},
                    .uvs = {122.0f / 256.0f, 26.0f / 256.0f, 137.0f / 256.0f, 50.0f / 256.0f},
                },
            ['>'] =
                {
                    .size = {16, 24},
                    .uvs = {139.0f / 256.0f, 26.0f / 256.0f, 155.0f / 256.0f, 50.0f / 256.0f},
                },
            ['?'] =
                {
                    .size = {12, 24},
                    .uvs = {157.0f / 256.0f, 26.0f / 256.0f, 169.0f / 256.0f, 50.0f / 256.0f},
                },
            ['@'] =
                {
                    .size = {23, 24},
                    .uvs = {171.0f / 256.0f, 26.0f / 256.0f, 194.0f / 256.0f, 50.0f / 256.0f},
                },
            ['A'] =
                {
                    .size = {12, 24},
                    .uvs = {196.0f / 256.0f, 26.0f / 256.0f, 208.0f / 256.0f, 50.0f / 256.0f},
                },
            ['B'] =
                {
                    .size = {12, 24},
                    .uvs = {210.0f / 256.0f, 26.0f / 256.0f, 222.0f / 256.0f, 50.0f / 256.0f},
                },
            ['C'] =
                {
                    .size = {12, 24},
                    .uvs = {224.0f / 256.0f, 26.0f / 256.0f, 236.0f / 256.0f, 50.0f / 256.0f},
                },
            ['D'] =
                {
                    .size = {13, 24},
                    .uvs = {238.0f / 256.0f, 26.0f / 256.0f, 251.0f / 256.0f, 50.0f / 256.0f},
                },
            ['E'] =
                {
                    .size = {11, 24},
                    .uvs = {1.0f / 256.0f, 51.0f / 256.0f, 12.0f / 256.0f, 75.0f / 256.0f},
                },
            ['F'] =
                {
                    .size = {11, 24},
                    .uvs = {14.0f / 256.0f, 51.0f / 256.0f, 25.0f / 256.0f, 75.0f / 256.0f},
                },
            ['G'] =
                {
                    .size = {12, 24},
                    .uvs = {27.0f / 256.0f, 51.0f / 256.0f, 39.0f / 256.0f, 75.0f / 256.0f},
                },
            ['H'] =
                {
                    .size = {12, 24},
                    .uvs = {41.0f / 256.0f, 51.0f / 256.0f, 53.0f / 256.0f, 75.0f / 256.0f},
                },
            ['I'] =
                {
                    .size = {8, 24},
                    .uvs = {55.0f / 256.0f, 51.0f / 256.0f, 63.0f / 256.0f, 75.0f / 256.0f},
                },
            ['J'] =
                {
                    .size = {10, 24},
                    .uvs = {65.0f / 256.0f, 51.0f / 256.0f, 75.0f / 256.0f, 75.0f / 256.0f},
                },
            ['K'] =
                {
                    .size = {13, 24},
                    .uvs = {77.0f / 256.0f, 51.0f / 256.0f, 90.0f / 256.0f, 75.0f / 256.0f},
                },
            ['L'] =
                {
                    .size = {11, 24},
                    .uvs = {91.0f / 256.0f, 51.0f / 256.0f, 102.0f / 256.0f, 75.0f / 256.0f},
                },
            ['M'] =
                {
                    .size = {16, 24},
                    .uvs = {104.0f / 256.0f, 51.0f / 256.0f, 120.0f / 256.0f, 75.0f / 256.0f},
                },
            ['N'] =
                {
                    .size = {13, 24},
                    .uvs = {122.0f / 256.0f, 51.0f / 256.0f, 135.0f / 256.0f, 75.0f / 256.0f},
                },
            ['O'] =
                {
                    .size = {13, 24},
                    .uvs = {137.0f / 256.0f, 51.0f / 256.0f, 150.0f / 256.0f, 75.0f / 256.0f},
                },
            ['P'] =
                {
                    .size = {13, 24},
                    .uvs = {152.0f / 256.0f, 51.0f / 256.0f, 165.0f / 256.0f, 75.0f / 256.0f},
                },
            ['Q'] =
                {
                    .size = {13, 24},
                    .uvs = {167.0f / 256.0f, 51.0f / 256.0f, 180.0f / 256.0f, 75.0f / 256.0f},
                },
            ['R'] =
                {
                    .size = {13, 24},
                    .uvs = {182.0f / 256.0f, 51.0f / 256.0f, 195.0f / 256.0f, 75.0f / 256.0f},
                },
            ['S'] =
                {
                    .size = {11, 24},
                    .uvs = {197.0f / 256.0f, 51.0f / 256.0f, 208.0f / 256.0f, 75.0f / 256.0f},
                },
            ['T'] =
                {
                    .size = {13, 24},
                    .uvs = {210.0f / 256.0f, 51.0f / 256.0f, 223.0f / 256.0f, 75.0f / 256.0f},
                },
            ['U'] =
                {
                    .size = {13, 24},
                    .uvs = {225.0f / 256.0f, 51.0f / 256.0f, 238.0f / 256.0f, 75.0f / 256.0f},
                },
            ['V'] =
                {
                    .size = {13, 24},
                    .uvs = {240.0f / 256.0f, 51.0f / 256.0f, 253.0f / 256.0f, 75.0f / 256.0f},
                },
            ['W'] =
                {
                    .size = {17, 24},
                    .uvs = {1.0f / 256.0f, 76.0f / 256.0f, 18.0f / 256.0f, 100.0f / 256.0f},
                },
            ['X'] =
                {
                    .size = {14, 24},
                    .uvs = {20.0f / 256.0f, 76.0f / 256.0f, 34.0f / 256.0f, 100.0f / 256.0f},
                },
            ['Y'] =
                {
                    .size = {15, 24},
                    .uvs = {36.0f / 256.0f, 76.0f / 256.0f, 51.0f / 256.0f, 100.0f / 256.0f},
                },
            ['Z'] =
                {
                    .size = {11, 24},
                    .uvs = {53.0f / 256.0f, 76.0f / 256.0f, 64.0f / 256.0f, 100.0f / 256.0f},
                },
            ['['] =
                {
                    .size = {9, 24},
                    .uvs = {66.0f / 256.0f, 76.0f / 256.0f, 75.0f / 256.0f, 100.0f / 256.0f},
                },
            ['\\'] =
                {
                    .size = {12, 24},
                    .uvs = {77.0f / 256.0f, 76.0f / 256.0f, 89.0f / 256.0f, 100.0f / 256.0f},
                },
            [']'] =
                {
                    .size = {9, 24},
                    .uvs = {91.0f / 256.0f, 76.0f / 256.0f, 100.0f / 256.0f, 100.0f / 256.0f},
                },
            ['^'] =
                {
                    .size = {14, 24},
                    .uvs = {102.0f / 256.0f, 76.0f / 256.0f, 116.0f / 256.0f, 100.0f / 256.0f},
                },
            ['_'] =
                {
                    .size = {15, 24},
                    .uvs = {118.0f / 256.0f, 76.0f / 256.0f, 133.0f / 256.0f, 100.0f / 256.0f},
                },
            ['`'] =
                {
                    .size = {7, 24},
                    .uvs = {135.0f / 256.0f, 76.0f / 256.0f, 142.0f / 256.0f, 100.0f / 256.0f},
                },
            ['a'] =
                {
                    .size = {12, 24},
                    .uvs = {144.0f / 256.0f, 76.0f / 256.0f, 156.0f / 256.0f, 100.0f / 256.0f},
                },
            ['b'] =
                {
                    .size = {12, 24},
                    .uvs = {158.0f / 256.0f, 76.0f / 256.0f, 170.0f / 256.0f, 100.0f / 256.0f},
                },
            ['c'] =
                {
                    .size = {10, 24},
                    .uvs = {172.0f / 256.0f, 76.0f / 256.0f, 182.0f / 256.0f, 100.0f / 256.0f},
                },
            ['d'] =
                {
                    .size = {13, 24},
                    .uvs = {184.0f / 256.0f, 76.0f / 256.0f, 197.0f / 256.0f, 100.0f / 256.0f},
                },
            ['e'] =
                {
                    .size = {12, 24},
                    .uvs = {199.0f / 256.0f, 76.0f / 256.0f, 211.0f / 256.0f, 100.0f / 256.0f},
                },
            ['f'] =
                {
                    .size = {10, 24},
                    .uvs = {213.0f / 256.0f, 76.0f / 256.0f, 223.0f / 256.0f, 100.0f / 256.0f},
                },
            ['g'] =
                {
                    .size = {11, 24},
                    .uvs = {225.0f / 256.0f, 76.0f / 256.0f, 236.0f / 256.0f, 100.0f / 256.0f},
                },
            ['h'] =
                {
                    .size = {12, 24},
                    .uvs = {238.0f / 256.0f, 76.0f / 256.0f, 250.0f / 256.0f, 100.0f / 256.0f},
                },
            ['i'] =
                {
                    .size = {8, 24},
                    .uvs = {1.0f / 256.0f, 101.0f / 256.0f, 9.0f / 256.0f, 125.0f / 256.0f},
                },
            ['j'] =
                {
                    .size = {8, 24},
                    .uvs = {11.0f / 256.0f, 101.0f / 256.0f, 19.0f / 256.0f, 125.0f / 256.0f},
                },
            ['k'] =
                {
                    .size = {13, 24},
                    .uvs = {21.0f / 256.0f, 101.0f / 256.0f, 34.0f / 256.0f, 125.0f / 256.0f},
                },
            ['l'] =
                {
                    .size = {8, 24},
                    .uvs = {36.0f / 256.0f, 101.0f / 256.0f, 44.0f / 256.0f, 125.0f / 256.0f},
                },
            ['m'] =
                {
                    .size = {15, 24},
                    .uvs = {46.0f / 256.0f, 101.0f / 256.0f, 61.0f / 256.0f, 125.0f / 256.0f},
                },
            ['n'] =
                {
                    .size = {13, 24},
                    .uvs = {63.0f / 256.0f, 101.0f / 256.0f, 76.0f / 256.0f, 125.0f / 256.0f},
                },
            ['o'] =
                {
                    .size = {12, 24},
                    .uvs = {78.0f / 256.0f, 101.0f / 256.0f, 90.0f / 256.0f, 125.0f / 256.0f},
                },
            ['p'] =
                {
                    .size = {12, 24},
                    .uvs = {92.0f / 256.0f, 101.0f / 256.0f, 104.0f / 256.0f, 125.0f / 256.0f},
                },
            ['q'] =
                {
                    .size = {12, 24},
                    .uvs = {106.0f / 256.0f, 101.0f / 256.0f, 118.0f / 256.0f, 125.0f / 256.0f},
                },
            ['r'] =
                {
                    .size = {10, 24},
                    .uvs = {120.0f / 256.0f, 101.0f / 256.0f, 130.0f / 256.0f, 125.0f / 256.0f},
                },
            ['s'] =
                {
                    .size = {9, 24},
                    .uvs = {132.0f / 256.0f, 101.0f / 256.0f, 141.0f / 256.0f, 125.0f / 256.0f},
                },
            ['t'] =
                {
                    .size = {10, 24},
                    .uvs = {143.0f / 256.0f, 101.0f / 256.0f, 153.0f / 256.0f, 125.0f / 256.0f},
                },
            ['u'] =
                {
                    .size = {12, 24},
                    .uvs = {155.0f / 256.0f, 101.0f / 256.0f, 167.0f / 256.0f, 125.0f / 256.0f},
                },
            ['v'] =
                {
                    .size = {11, 24},
                    .uvs = {169.0f / 256.0f, 101.0f / 256.0f, 180.0f / 256.0f, 125.0f / 256.0f},
                },
            ['w'] =
                {
                    .size = {15, 24},
                    .uvs = {182.0f / 256.0f, 101.0f / 256.0f, 197.0f / 256.0f, 125.0f / 256.0f},
                },
            ['x'] =
                {
                    .size = {12, 24},
                    .uvs = {199.0f / 256.0f, 101.0f / 256.0f, 211.0f / 256.0f, 125.0f / 256.0f},
                },
            ['y'] =
                {
                    .size = {13, 24},
                    .uvs = {213.0f / 256.0f, 101.0f / 256.0f, 226.0f / 256.0f, 125.0f / 256.0f},
                },
            ['z'] =
                {
                    .size = {11, 24},
                    .uvs = {228.0f / 256.0f, 101.0f / 256.0f, 239.0f / 256.0f, 125.0f / 256.0f},
                },
            ['{'] =
                {
                    .size = {12, 24},
                    .uvs = {241.0f / 256.0f, 101.0f / 256.0f, 253.0f / 256.0f, 125.0f / 256.0f},
                },
            ['|'] =
                {
                    .size = {7, 24},
                    .uvs = {1.0f / 256.0f, 126.0f / 256.0f, 8.0f / 256.0f, 150.0f / 256.0f},
                },
            ['}'] =
                {
                    .size = {12, 24},
                    .uvs = {10.0f / 256.0f, 126.0f / 256.0f, 22.0f / 256.0f, 150.0f / 256.0f},
                },
            ['~'] =
                {
                    .size = {17, 24},
                    .uvs = {24.0f / 256.0f, 126.0f / 256.0f, 41.0f / 256.0f, 150.0f / 256.0f},
                },
        },
    },

    [FNT_HUD] = {
        .tname = "F_HUD",
        .spacing = 1,
        .line_height = 18,
        .glyphs = {
            [' '] =
                {
                    .size = {5, 16},
                    .uvs = {0},
                },
            ['A'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 1.0f / 128.0f, 16.0f / 128.0f, 17.0f / 128.0f},
                },
            ['B'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 1.0f / 128.0f, 33.0f / 128.0f, 17.0f / 128.0f},
                },
            ['C'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 1.0f / 128.0f, 50.0f / 128.0f, 17.0f / 128.0f},
                },
            ['D'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 1.0f / 128.0f, 67.0f / 128.0f, 17.0f / 128.0f},
                },
            ['E'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 1.0f / 128.0f, 84.0f / 128.0f, 17.0f / 128.0f},
                },
            ['F'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 1.0f / 128.0f, 101.0f / 128.0f, 17.0f / 128.0f},
                },
            ['G'] =
                {
                    .size = {15, 16},
                    .uvs = {103.0f / 128.0f, 1.0f / 128.0f, 118.0f / 128.0f, 17.0f / 128.0f},
                },
            ['H'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 19.0f / 128.0f, 16.0f / 128.0f, 35.0f / 128.0f},
                },
            ['I'] =
                {
                    .size = {11, 16},
                    .uvs = {18.0f / 128.0f, 19.0f / 128.0f, 29.0f / 128.0f, 35.0f / 128.0f},
                },
            ['J'] =
                {
                    .size = {15, 16},
                    .uvs = {31.0f / 128.0f, 19.0f / 128.0f, 46.0f / 128.0f, 35.0f / 128.0f},
                },
            ['K'] =
                {
                    .size = {15, 16},
                    .uvs = {48.0f / 128.0f, 19.0f / 128.0f, 63.0f / 128.0f, 35.0f / 128.0f},
                },
            ['L'] =
                {
                    .size = {13, 16},
                    .uvs = {65.0f / 128.0f, 19.0f / 128.0f, 78.0f / 128.0f, 35.0f / 128.0f},
                },
            ['M'] =
                {
                    .size = {15, 16},
                    .uvs = {80.0f / 128.0f, 19.0f / 128.0f, 95.0f / 128.0f, 35.0f / 128.0f},
                },
            ['N'] =
                {
                    .size = {15, 16},
                    .uvs = {97.0f / 128.0f, 19.0f / 128.0f, 112.0f / 128.0f, 35.0f / 128.0f},
                },
            ['O'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 37.0f / 128.0f, 16.0f / 128.0f, 53.0f / 128.0f},
                },
            ['P'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 37.0f / 128.0f, 33.0f / 128.0f, 53.0f / 128.0f},
                },
            ['Q'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 37.0f / 128.0f, 50.0f / 128.0f, 53.0f / 128.0f},
                },
            ['R'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 37.0f / 128.0f, 67.0f / 128.0f, 53.0f / 128.0f},
                },
            ['S'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 37.0f / 128.0f, 84.0f / 128.0f, 53.0f / 128.0f},
                },
            ['T'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 37.0f / 128.0f, 101.0f / 128.0f, 53.0f / 128.0f},
                },
            ['U'] =
                {
                    .size = {15, 16},
                    .uvs = {103.0f / 128.0f, 37.0f / 128.0f, 118.0f / 128.0f, 53.0f / 128.0f},
                },
            ['V'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 55.0f / 128.0f, 16.0f / 128.0f, 71.0f / 128.0f},
                },
            ['W'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 55.0f / 128.0f, 33.0f / 128.0f, 71.0f / 128.0f},
                },
            ['X'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 55.0f / 128.0f, 50.0f / 128.0f, 71.0f / 128.0f},
                },
            ['Y'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 55.0f / 128.0f, 67.0f / 128.0f, 71.0f / 128.0f},
                },
            ['Z'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 55.0f / 128.0f, 84.0f / 128.0f, 71.0f / 128.0f},
                },
            ['a'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 1.0f / 128.0f, 16.0f / 128.0f, 17.0f / 128.0f},
                },
            ['b'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 1.0f / 128.0f, 33.0f / 128.0f, 17.0f / 128.0f},
                },
            ['c'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 1.0f / 128.0f, 50.0f / 128.0f, 17.0f / 128.0f},
                },
            ['d'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 1.0f / 128.0f, 67.0f / 128.0f, 17.0f / 128.0f},
                },
            ['e'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 1.0f / 128.0f, 84.0f / 128.0f, 17.0f / 128.0f},
                },
            ['f'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 1.0f / 128.0f, 101.0f / 128.0f, 17.0f / 128.0f},
                },
            ['g'] =
                {
                    .size = {15, 16},
                    .uvs = {103.0f / 128.0f, 1.0f / 128.0f, 118.0f / 128.0f, 17.0f / 128.0f},
                },
            ['h'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 19.0f / 128.0f, 16.0f / 128.0f, 35.0f / 128.0f},
                },
            ['i'] =
                {
                    .size = {11, 16},
                    .uvs = {18.0f / 128.0f, 19.0f / 128.0f, 29.0f / 128.0f, 35.0f / 128.0f},
                },
            ['j'] =
                {
                    .size = {15, 16},
                    .uvs = {31.0f / 128.0f, 19.0f / 128.0f, 46.0f / 128.0f, 35.0f / 128.0f},
                },
            ['k'] =
                {
                    .size = {15, 16},
                    .uvs = {48.0f / 128.0f, 19.0f / 128.0f, 63.0f / 128.0f, 35.0f / 128.0f},
                },
            ['l'] =
                {
                    .size = {13, 16},
                    .uvs = {65.0f / 128.0f, 19.0f / 128.0f, 78.0f / 128.0f, 35.0f / 128.0f},
                },
            ['m'] =
                {
                    .size = {15, 16},
                    .uvs = {80.0f / 128.0f, 19.0f / 128.0f, 95.0f / 128.0f, 35.0f / 128.0f},
                },
            ['n'] =
                {
                    .size = {15, 16},
                    .uvs = {97.0f / 128.0f, 19.0f / 128.0f, 112.0f / 128.0f, 35.0f / 128.0f},
                },
            ['o'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 37.0f / 128.0f, 16.0f / 128.0f, 53.0f / 128.0f},
                },
            ['p'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 37.0f / 128.0f, 33.0f / 128.0f, 53.0f / 128.0f},
                },
            ['q'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 37.0f / 128.0f, 50.0f / 128.0f, 53.0f / 128.0f},
                },
            ['r'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 37.0f / 128.0f, 67.0f / 128.0f, 53.0f / 128.0f},
                },
            ['s'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 37.0f / 128.0f, 84.0f / 128.0f, 53.0f / 128.0f},
                },
            ['t'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 37.0f / 128.0f, 101.0f / 128.0f, 53.0f / 128.0f},
                },
            ['u'] =
                {
                    .size = {15, 16},
                    .uvs = {103.0f / 128.0f, 37.0f / 128.0f, 118.0f / 128.0f, 53.0f / 128.0f},
                },
            ['v'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 55.0f / 128.0f, 16.0f / 128.0f, 71.0f / 128.0f},
                },
            ['w'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 55.0f / 128.0f, 33.0f / 128.0f, 71.0f / 128.0f},
                },
            ['x'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 55.0f / 128.0f, 50.0f / 128.0f, 71.0f / 128.0f},
                },
            ['y'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 55.0f / 128.0f, 67.0f / 128.0f, 71.0f / 128.0f},
                },
            ['z'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 55.0f / 128.0f, 84.0f / 128.0f, 71.0f / 128.0f},
                },

            ['0'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 55.0f / 128.0f, 101.0f / 128.0f, 71.0f / 128.0f},
                },
            ['1'] =
                {
                    .size = {11, 16},
                    .uvs = {103.0f / 128.0f, 55.0f / 128.0f, 114.0f / 128.0f, 71.0f / 128.0f},
                },
            ['2'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 73.0f / 128.0f, 16.0f / 128.0f, 89.0f / 128.0f},
                },
            ['3'] =
                {
                    .size = {15, 16},
                    .uvs = {18.0f / 128.0f, 73.0f / 128.0f, 33.0f / 128.0f, 89.0f / 128.0f},
                },
            ['4'] =
                {
                    .size = {15, 16},
                    .uvs = {35.0f / 128.0f, 73.0f / 128.0f, 50.0f / 128.0f, 89.0f / 128.0f},
                },
            ['5'] =
                {
                    .size = {15, 16},
                    .uvs = {52.0f / 128.0f, 73.0f / 128.0f, 67.0f / 128.0f, 89.0f / 128.0f},
                },
            ['6'] =
                {
                    .size = {15, 16},
                    .uvs = {69.0f / 128.0f, 73.0f / 128.0f, 84.0f / 128.0f, 89.0f / 128.0f},
                },
            ['7'] =
                {
                    .size = {15, 16},
                    .uvs = {86.0f / 128.0f, 73.0f / 128.0f, 101.0f / 128.0f, 89.0f / 128.0f},
                },
            ['8'] =
                {
                    .size = {15, 16},
                    .uvs = {103.0f / 128.0f, 73.0f / 128.0f, 118.0f / 128.0f, 89.0f / 128.0f},
                },
            ['9'] =
                {
                    .size = {15, 16},
                    .uvs = {1.0f / 128.0f, 91.0f / 128.0f, 16.0f / 128.0f, 107.0f / 128.0f},
                },
            ['*'] =
                {
                    .size = {13, 16},
                    .uvs = {72.0f / 128.0f, 91.0f / 128.0f, 85.0f / 128.0f, 107.0f / 128.0f},
                },
            ['.'] = {
                .size = {7, 16},
                .uvs = {18.0f / 128.0f, 91.0f / 128.0f, 25.0f / 128.0f, 107.0f / 128.0f},
            },
        },
    },
};

static vec2 camera = {HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT};
static mat4 mvp = GLM_MAT4_IDENTITY_INIT;
static struct VertexBatch batch = {0};

static StTinyMap* tiles = NULL;
static struct TileBatch* valid_tiles = NULL;

static struct VideoState state = {0};

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
    glGenTextures(1, &blank_texture);
    glBindTexture(GL_TEXTURE_2D, blank_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, WHITE);

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
    batch.stencil = 0;
    batch.texture = blank_texture;
    batch.alpha_test = 0.5f;

    batch.blend_src[0] = batch.blend_src[1] = GL_SRC_ALPHA;
    batch.blend_dest[0] = GL_ONE_MINUS_SRC_ALPHA;
    batch.blend_dest[1] = GL_ONE;
    batch.logic = GL_COPY;

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
                                           "uniform float u_stencil;\n"
                                           "\n"
                                           "void main() {\n"
                                           "   vec4 sample = texture(u_texture, v_uv);\n"
                                           "   if (u_alpha_test > 0.0) {\n"
                                           "       if (sample.a < u_alpha_test)\n"
                                           "           discard;\n"
                                           "       sample.a = 1.0;\n"
                                           "   }\n"
                                           "\n"
                                           "   o_color.rgb = v_color.rgb * mix(sample.rgb, vec3(1.0), u_stencil);\n"
                                           "   o_color.a = v_color.a * sample.a;\n"
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
    uniforms.stencil = glGetUniformLocation(shader, "u_stencil");

    glEnable(GL_BLEND);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uniforms.texture, 0);
    glDepthFunc(GL_LEQUAL);

    textures = NewTinyMap();
    tiles = NewTinyMap();

    draw_time = SDL_GetTicks();
}

void video_update(const char* errmsg, const char* chat) {
    video_update_custom_start();

    struct TileBatch* tilemap = valid_tiles;
    if (tilemap != NULL) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
        glUniform1f(uniforms.alpha_test, 0.5f);
        while (tilemap != NULL) {
            glDepthMask(tilemap->translucent ? GL_FALSE : GL_TRUE);
            glBindVertexArray(tilemap->vao);
            glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(struct Vertex) * tilemap->vertex_count), tilemap->vertices
            );

            glBindTexture(GL_TEXTURE_2D, (tilemap->texture == NULL) ? blank_texture : tilemap->texture->texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tilemap->vertex_count);

            tilemap = tilemap->next;
        }
    }
    glDepthMask(GL_TRUE);

    draw_state();
    submit_batch();

    glDisable(GL_DEPTH_TEST);

    glm_ortho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -16000, 16000, mvp);
    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)mvp);

    if (errmsg == NULL) {
        draw_state_hud();
        if (chat != NULL)
            draw_text(FNT_MAIN, FA_LEFT, chat, (float[3]){32, SCREEN_HEIGHT - 50, 0});
    } else {
        draw_rectangle("", (float[2][2]){{0, 0}, {SCREEN_WIDTH, SCREEN_HEIGHT}}, 0, RGBA(0, 0, 0, 128));
        draw_text(FNT_MAIN, FA_LEFT, errmsg, (float[3]){16, 16, 0});
    }

    video_update_custom_end();
}

void video_update_custom_start() {
    const uint64_t last_draw_time = draw_time;
    draw_time = SDL_GetTicks();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vec2 cam;
    if (state.lerp_time[0] < state.lerp_time[1]) {
        state.lerp_time[0] += draw_time - last_draw_time;
        glm_vec2_lerp(state.lerp, camera, (float)(state.lerp_time[0]) / (float)(state.lerp_time[1]), cam);
    } else
        glm_vec2_copy(camera, cam);
    glm_ortho(
        cam[0] - HALF_SCREEN_WIDTH, cam[0] + HALF_SCREEN_WIDTH, cam[1] + HALF_SCREEN_HEIGHT,
        cam[1] - HALF_SCREEN_HEIGHT, -1000, 1000, mvp
    );
    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)mvp);
}

void video_update_custom_end() {
    submit_batch();
    SDL_GL_SwapWindow(window);
}

void video_teardown() {
    glDeleteVertexArrays(1, &(batch.vao));
    glDeleteBuffers(1, &(batch.vbo));
    FreeTinyMap(tiles);
    glDeleteTextures(1, &blank_texture);
    FreeTinyMap(textures);
    glDeleteProgram(shader);
    SDL_free(batch.vertices);

    SDL_GL_DestroyContext(gpu);
    SDL_DestroyWindow(window);
}

void save_video_state(struct VideoState* to) {
    SDL_memcpy(to, &state, sizeof(state));
}

void load_video_state(const struct VideoState* from) {
    SDL_memcpy(&state, from, sizeof(state));
}

void tick_video_state() {
    if (state.quake > 0.0f)
        state.quake -= 1.0f;
}

void quake_at(float pos[2], float amount) {
    const float dist = glm_vec2_distance(camera, pos) / (float)SCREEN_HEIGHT;
    state.quake += amount / SDL_max(dist, 1.0f);
}

float get_quake() {
    return state.quake;
}

void move_camera(float x, float y) {
    camera[0] = x;
    camera[1] = y;
    move_ears(x, y);
}

void lerp_camera(uint64_t ticks) {
    glm_vec2_copy(camera, state.lerp);
    state.lerp_time[0] = 0;
    state.lerp_time[1] = ticks;
}

static void nuke_texture(void* ptr) {
    glDeleteTextures(1, &(((struct Texture*)ptr)->texture));
}

void load_texture(const char* index) {
    if (index[0] == '\0' || get_texture(index) != NULL)
        return;

    struct Texture texture = {0};

    const char* file = find_file(file_pattern("data/textures/%s.*", index), ".json");
    if (file == NULL)
        FATAL("Texture \"%s\" not found", index);
    SDL_Surface* surface = IMG_Load(file);
    if (surface == NULL)
        FATAL("Texture \"%s\" fail: %s", index, SDL_GetError());

    SDL_strlcpy(texture.name, index, sizeof(texture.name));
    texture.size[0] = surface->w;
    texture.size[1] = surface->h;

    glGenTextures(1, &(texture.texture));
    glBindTexture(GL_TEXTURE_2D, texture.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    GLint format;
    switch (surface->format) {
        default: {
            SDL_Surface* temp = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            if (temp == NULL)
                FATAL("Texture \"%s\" conversion fail: %s", index, SDL_GetError());
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

    file = find_file(file_pattern("data/textures/%s.json", index), NULL);
    if (file != NULL) {
        yyjson_doc* json =
            yyjson_read_file(file, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL);
        if (json != NULL) {
            yyjson_val* root = yyjson_doc_get_root(json);
            if (yyjson_is_obj(root)) {
                texture.offset[0] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "x_offset"));
                texture.offset[1] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "y_offset"));
            }
            yyjson_doc_free(json);
        }
    }

    const StTinyKey key = StStrKey(index);
    StMapPut(textures, key, &texture, sizeof(texture));
    StMapFind(textures, key)->cleanup = nuke_texture;
}

const struct Texture* get_texture(const char* index) {
    return (struct Texture*)StMapGet(textures, StStrKey(index));
}

void load_font(enum FontIndices index) {
    if (fonts[index].texture != NULL)
        return;
    load_texture(fonts[index].tname);
    fonts[index].texture = get_texture(fonts[index].tname);
}

void set_batch_stencil(GLfloat stencil) {
    if (batch.stencil != stencil) {
        submit_batch();
        batch.stencil = stencil;
    }
}

void set_batch_logic(GLenum opcode) {
    if (batch.logic != opcode) {
        submit_batch();
        if (opcode == GL_COPY)
            glDisable(GL_COLOR_LOGIC_OP);
        else
            glEnable(GL_COLOR_LOGIC_OP);
        batch.logic = opcode;
    }
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
    glUniform1f(uniforms.stencil, batch.stencil);

    // Apply blend mode
    glBlendFuncSeparate(batch.blend_src[0], batch.blend_dest[0], batch.blend_src[1], batch.blend_dest[1]);
    glLogicOp(batch.logic);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
    batch.vertex_count = 0;
}

void clear_tiles() {
    FreeTinyMap(tiles);
    tiles = NewTinyMap();
    valid_tiles = NULL;
}

static void nuke_tiles(void* ptr) {
    struct TileBatch* tilemap = (struct TileBatch*)ptr;
    if (valid_tiles == tilemap)
        valid_tiles = tilemap->next;
    glDeleteVertexArrays(1, &(tilemap->vao));
    glDeleteBuffers(1, &(tilemap->vbo));
    SDL_free(tilemap->vertices);
}

static struct TileBatch* load_tile(const char* index) {
    const struct Texture* texture = NULL;
    if (index != NULL) {
        load_texture(index);
        texture = get_texture(index);
    }

    const StTinyKey key = StStrKey((index == NULL) ? "" : index);
    struct TileBatch* tilemap = StMapGet(tiles, key);
    if (tilemap != NULL)
        return tilemap;

    struct TileBatch new = {0};
    new.next = valid_tiles;
    new.texture = texture;
    new.translucent = false;

    glGenVertexArrays(1, &(new.vao));
    glBindVertexArray(new.vao);
    glEnableVertexArrayAttrib(new.vao, VATT_POSITION);
    glVertexArrayAttribFormat(new.vao, VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3);
    glEnableVertexArrayAttrib(new.vao, VATT_COLOR);
    glVertexArrayAttribFormat(new.vao, VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLubyte) * 4);
    glEnableVertexArrayAttrib(new.vao, VATT_UV);
    glVertexArrayAttribFormat(new.vao, VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);

    new.vertex_count = 0;
    new.vertex_capacity = 6;
    new.vertices = SDL_malloc(new.vertex_capacity * sizeof(struct Vertex));
    if (new.vertices == NULL)
        FATAL("Out of memory for tile batch \"%s\" vertices", index);

    glGenBuffers(1, &(new.vbo));
    glBindBuffer(GL_ARRAY_BUFFER, new.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(struct Vertex) * new.vertex_capacity), NULL, GL_STATIC_DRAW);

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

    StMapPut(tiles, key, &new, sizeof(new));
    tilemap = StMapGet(tiles, key);
    if (tilemap == NULL)
        FATAL("Out of memory for tile batch \"%s\"", index);
    StMapFind(tiles, key)->cleanup = nuke_tiles;

    return (valid_tiles = tilemap);
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

void add_gradient(const char* index, const GLfloat rect[2][2], GLfloat z, const GLubyte color[4][4]) {
    struct TileBatch* tilemap = load_tile(index);
    const struct Texture* texture = tilemap->texture;
    const GLfloat u = (texture != NULL) ? ((rect[1][0] - rect[0][0]) / (GLfloat)(texture->size[0])) : 1;
    const GLfloat v = (texture != NULL) ? ((rect[1][1] - rect[0][1]) / (GLfloat)(texture->size[1])) : 1;

    tile_vertex(tilemap, rect[0][0], rect[1][1], z, color[2][0], color[2][1], color[2][2], color[2][3], 0, v);
    tile_vertex(tilemap, rect[0][0], rect[0][1], z, color[0][0], color[0][1], color[0][2], color[0][3], 0, 0);
    tile_vertex(tilemap, rect[1][0], rect[0][1], z, color[1][0], color[1][1], color[1][2], color[1][3], u, 0);
    tile_vertex(tilemap, rect[1][0], rect[0][1], z, color[1][0], color[1][1], color[1][2], color[1][3], u, 0);
    tile_vertex(tilemap, rect[1][0], rect[1][1], z, color[3][0], color[3][1], color[3][2], color[3][3], u, v);
    tile_vertex(tilemap, rect[0][0], rect[1][1], z, color[2][0], color[2][1], color[2][2], color[2][3], 0, v);

    if (color[0][3] < 255 || color[1][3] < 255 || color[2][3] < 255 || color[3][3] < 255)
        tilemap->translucent = true;
}

void add_backdrop(const char* index, const GLfloat pos[3], const GLfloat scale[2], const GLubyte color[4]) {
    struct TileBatch* tilemap = load_tile(index);
    const struct Texture* texture = get_texture(index);

    const GLfloat x1 = pos[0] - (texture->offset[0] * scale[0]);
    const GLfloat y1 = pos[1] - (texture->offset[1] * scale[1]);
    const GLfloat x2 = x1 + ((GLfloat)texture->size[0] * scale[0]);
    const GLfloat y2 = y1 + ((GLfloat)texture->size[1] * scale[1]);

    tile_vertex(tilemap, x1, y2, pos[2], color[0], color[1], color[2], color[3], 0, 1);
    tile_vertex(tilemap, x1, y1, pos[2], color[0], color[1], color[2], color[3], 0, 0);
    tile_vertex(tilemap, x2, y1, pos[2], color[0], color[1], color[2], color[3], 1, 0);
    tile_vertex(tilemap, x2, y1, pos[2], color[0], color[1], color[2], color[3], 1, 0);
    tile_vertex(tilemap, x2, y2, pos[2], color[0], color[1], color[2], color[3], 1, 1);
    tile_vertex(tilemap, x1, y2, pos[2], color[0], color[1], color[2], color[3], 0, 1);

    if (color[3] < 255)
        tilemap->translucent = true;
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

void draw_sprite(const char* index, const GLfloat pos[3], const bool flip[2], GLfloat angle, const GLubyte color[4]) {
    const struct Texture* texture = get_texture(index);
    if (texture == NULL)
        return;

    if (batch.texture != texture->texture) {
        submit_batch();
        batch.texture = texture->texture;
    }

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
        glm_vec2_rotate(p1, angle, p1);
        glm_vec2_rotate(p2, angle, p2);
        glm_vec2_rotate(p3, angle, p3);
        glm_vec2_rotate(p4, angle, p4);
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

GLfloat string_width(enum FontIndices index, const char* str) {
    struct Font* font = &(fonts[index]);
    GLfloat width = 0;

    GLfloat cx = 0;
    size_t bytes = SDL_strlen(str);
    while (bytes > 0) {
        size_t gid = SDL_StepUTF8(&str, &bytes);

        // Special/invalid characters
        if (gid == '\r')
            continue;
        if (gid == '\n') {
            cx = 0;
            continue;
        }
        if (SDL_isspace((int)gid))
            gid = ' ';
        if (gid >= 256)
            continue;

        // Valid glyph
        struct Glyph* glyph = &(font->glyphs[gid]);
        cx += glyph->size[0];
        if (bytes > 0)
            cx += font->spacing;
        width = SDL_max(cx, width);
    }

    return width;
}

GLfloat string_height(enum FontIndices index, const char* str) {
    struct Font* font = &(fonts[index]);

    size_t bytes = SDL_strlen(str);
    GLfloat height = (bytes > 0) ? font->line_height : 0;
    while (bytes > 0)
        if (SDL_StepUTF8(&str, &bytes) == '\n')
            height += font->line_height;

    return height;
}

void draw_text(enum FontIndices index, enum FontAlignment align, const char* str, const float pos[3]) {
    draw_text_ext(index, align, str, pos, 1, WHITE);
}

void draw_text_ext(
    enum FontIndices index, enum FontAlignment align, const char* str, const float pos[3], float scale,
    const GLubyte color[4]
) {
    const struct Font* font = &(fonts[index]);

    const struct Texture* texture = font->texture;
    if (texture == NULL)
        FATAL("Invalid font texture \"%s\"", font->tname);
    if (batch.texture != texture->texture) {
        submit_batch();
        batch.texture = texture->texture;
    }

    GLfloat sx = pos[0];
    switch (align) {
        default:
            break;
        case FA_CENTER:
            sx -= string_width(index, str) * 0.5f * scale;
            break;
        case FA_RIGHT:
            sx -= string_width(index, str) * scale;
            break;
    }

    GLfloat cx = sx, cy = pos[1];
    size_t bytes = SDL_strlen(str);
    while (bytes > 0) {
        size_t gid = SDL_StepUTF8(&str, &bytes);

        // Special/invalid characters
        if (gid == '\r')
            continue;
        if (gid == '\n') {
            cx = sx;
            cy += font->line_height * scale;
            continue;
        }
        if (SDL_isspace((int)gid))
            gid = ' ';
        if (gid >= 256)
            continue;

        // Valid glyph
        const struct Glyph* glyph = &(font->glyphs[gid]);
        const GLfloat x1 = cx;
        const GLfloat y1 = cy;
        const GLfloat x2 = x1 + (glyph->size[0] * scale);
        const GLfloat y2 = y1 + (glyph->size[1] * scale);
        batch_vertex(x1, y2, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[0], glyph->uvs[3]);
        batch_vertex(x1, y1, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[0], glyph->uvs[1]);
        batch_vertex(x2, y1, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[2], glyph->uvs[1]);
        batch_vertex(x2, y1, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[2], glyph->uvs[1]);
        batch_vertex(x2, y2, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[2], glyph->uvs[3]);
        batch_vertex(x1, y2, pos[2], color[0], color[1], color[2], color[3], glyph->uvs[0], glyph->uvs[3]);

        cx += glyph->size[0] * scale;
        if (bytes > 0)
            cx += font->spacing * scale;
    }
}

void draw_rectangle(const char* index, const float rect[2][2], float z, const GLubyte color[4]) {
    const struct Texture* texture = get_texture(index);

    GLuint tex = (texture == NULL) ? blank_texture : texture->texture;
    if (batch.texture != tex) {
        submit_batch();
        batch.texture = tex;
    }

    GLfloat u, v;
    if (texture == NULL) {
        u = v = 1;
    } else {
        u = (rect[1][0] - rect[0][0]) / (GLfloat)(texture->size[0]);
        v = (rect[1][1] - rect[0][1]) / (GLfloat)(texture->size[1]);
    }

    batch_vertex(rect[0][0], rect[1][1], z, color[0], color[1], color[2], color[3], 0, v);
    batch_vertex(rect[0][0], rect[0][1], z, color[0], color[1], color[2], color[3], 0, 0);
    batch_vertex(rect[1][0], rect[0][1], z, color[0], color[1], color[2], color[3], u, 0);
    batch_vertex(rect[1][0], rect[0][1], z, color[0], color[1], color[2], color[3], u, 0);
    batch_vertex(rect[1][0], rect[1][1], z, color[0], color[1], color[2], color[3], u, v);
    batch_vertex(rect[0][0], rect[1][1], z, color[0], color[1], color[2], color[3], 0, v);
}

void draw_ellipse(const float rect[2][2], float z, const GLubyte color[4]) {
    if (batch.texture != blank_texture) {
        submit_batch();
        batch.texture = blank_texture;
    }

    const GLfloat x = glm_lerp(rect[0][0], rect[1][0], 0.5f);
    const GLfloat y = glm_lerp(rect[0][1], rect[1][1], 0.5f);
    const GLfloat nx = SDL_fabsf(rect[1][0] - rect[0][0]) * 0.5f;
    const GLfloat ny = SDL_fabsf(rect[1][1] - rect[0][1]) * 0.5f;

    for (int i = 0; i < 32; i++) {
        const GLfloat dir1 = (GLM_PIf * 2.0f) * ((float)i / 32.0f);
        const GLfloat dir2 = (GLM_PIf * 2.0f) * ((float)(i + 1) / 32.0f);

        batch_vertex(x, y, z, color[0], color[1], color[2], color[3], 0, 0);
        batch_vertex(x + SDL_cosf(dir1) * nx, y - SDL_sinf(dir1) * ny, z, color[0], color[1], color[2], color[3], 0, 0);
        batch_vertex(x + SDL_cosf(dir2) * nx, y - SDL_sinf(dir2) * ny, z, color[0], color[1], color[2], color[3], 0, 0);
    }
}
