#pragma once

#include <SDL3/SDL_stdinc.h>

// FUCK YOU `minwindef.h`
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif

typedef Uint8 Bool;
#define FALSE ((Bool)0)
#define TRUE ((Bool)1)
