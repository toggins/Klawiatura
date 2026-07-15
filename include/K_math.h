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

FVec2 Vadd(register FVec2, register FVec2), Vsub(register FVec2, register FVec2), Vmul(register FVec2, register FVec2),
    Vdiv(register FVec2, register FVec2);
FVec2 Vscale(register FVec2, register Fixed);
Fixed Vdist(register FVec2, register FVec2), Vtheta(register FVec2, register FVec2);
FVec2 Vlerp(register FVec2, register FVec2, register Fixed);

FRect Radd(register FRect, register FVec2), Rmul(register FRect, register FVec2);
FRect Rxflip(register FRect), Ryflip(register FRect);
FVec2 Rcenter(register FRect);
Bool Rcollide(register FRect, register FRect);
