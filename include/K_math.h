#pragma once

#include <SDL3/SDL_stdinc.h>

#define FIX_NOSTD
#include <S_fixed.h>

// FUCK YOU `minwindef.h`
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif

typedef Uint8 Bool;
#define FALSE ((Bool)0L)
#define TRUE ((Bool)1L)

typedef fix16_t Fixed;

typedef struct {
	Fixed x, y;
} FVec2;

typedef struct {
	FVec2 start, end;
} FRect;

FVec2 Vadd(register FVec2, register FVec2), Vsub(register FVec2, register FVec2);
FVec2 Vscale(register FVec2, register Fixed);
Fixed Vdist(register FVec2, register FVec2), Vtheta(register FVec2, register FVec2);
Bool Rcollide(FRect, FRect);
