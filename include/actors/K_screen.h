#pragma once

#include "K_game.h"

typedef Uint8 TextAnimations;
enum {
    TXTA_FADE,
    TXTA_LINEAR,
    TXTA_REAPPEAR,
    TXTA_SMOOTH,
};

enum {
    VAL_SCROLL_TRACK,

    VAL_TEXT_SECRET = 0,
    VAL_TEXT_ANIMATION,
    VAL_TEXT_ALPHA,
};

#define FLG_SCROLL_BOWSER CUSTOM_FLAG(0)
#define FLG_SCROLL_TANKS CUSTOM_FLAG(1)
