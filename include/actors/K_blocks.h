#pragma once

#include "K_game.h"

typedef Uint8 BlockTypes;
enum {
    BLOCK_ITEM,
    BLOCK_BRICK,
};

enum {
    VAL_BLOCK_TYPE,
    VAL_BLOCK_ITEM,
    VAL_BLOCK_BUMP,
    VAL_BLOCK_TIME,
};

#define FLG_BLOCK_REPEAT CUSTOM_FLAG(0)
#define FLG_BLOCK_HIDDEN CUSTOM_FLAG(1)
#define FLG_BLOCK_GRAY CUSTOM_FLAG(2)
#define FLG_BLOCK_EMPTY CUSTOM_FLAG(3)
