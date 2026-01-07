#pragma once

#include <SDL3/SDL_stdinc.h>

#include "K_math.h"

#define TICKRATE 50L

Bool got_ticks();
void from_scratch(), new_frame(float), next_tick();
float dt(), totalticks(), pendingticks();
