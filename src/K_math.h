#pragma once

#include <SDL3/SDL_stdinc.h>
#include <S_fixed.h>

typedef fix16_t fvec2[2];

#define FVEC2_ZERO ((fvec2){FxZero, FxZero})

void fvec2_zero(fvec2);
void fvec2_copy(const fvec2, fvec2);
void fvec2_lerp(const fvec2, const fvec2, fix16_t, fvec2);
bool fvec2_equal(const fvec2, const fvec2);
