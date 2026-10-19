#pragma once

#include "K_game.h"

typedef Uint8 ThwompAnimations;
enum {
    TA_IDLE,
    TA_BLINK,
    TA_LAUGH,
};

enum {
    VAL_THWOMP_Y,
    VAL_THWOMP_WAIT,
    VAL_THWOMP_OVERLAP,
    VAL_THWOMP_ANIMATION,
    VAL_THWOMP_FRAME,
};

#define FLG_THWOMP_FALL CUSTOM_FLAG(0)
#define FLG_THWOMP_FELL CUSTOM_FLAG(1)
#define FLG_THWOMP_HIT_BRICK CUSTOM_FLAG(2)
