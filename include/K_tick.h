#pragma once

#include <SDL3/SDL_stdinc.h>

#define TICKRATE 50

bool got_ticks();
void from_scratch(), new_frame(float), next_tick();
float dt(), totalticks(), pendingticks();
