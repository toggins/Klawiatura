#pragma once

#include "K_misc.h"

#define FIX_NOSTD
#include <S_fixed.h>

typedef struct {
    Fixed x, y;
} FVec2;

typedef struct {
    FVec2 start, end;
} FRect;

FVec2 Vadd(FVec2, FVec2), Vsub(FVec2, FVec2), Vmul(FVec2, FVec2), Vdiv(FVec2, FVec2);
FVec2 Vscale(FVec2, Fixed);
Fixed Vdist(FVec2, FVec2), Vtheta(FVec2, FVec2);
FVec2 Vlerp(FVec2, FVec2, Fixed), Vclamp(FVec2, FVec2, FVec2);

FRect Radd(FRect, FVec2), Rmul(FRect, FVec2);
FRect Rxflip(FRect), Ryflip(FRect);
FVec2 Rcenter(FRect);
Bool Rcollide(FRect, FRect);
