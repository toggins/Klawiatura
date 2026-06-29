#pragma once

#include "K_misc.h"

typedef struct {
    float size;
    const char *title, *text, *(*fmt)(), *verb, *font;
    Bool (*wait)();
    void (*finish)(), (*cancel)();
} UIMessageData;
