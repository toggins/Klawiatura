#include <SDL3/SDL_stdinc.h>

#include <S_fixed.h>

typedef uint8_t Bool;
typedef fix16_t fixed;

typedef struct {
	fixed x, y;
} fvec2;

typedef struct {
	fvec2 start, end;
} frect;

fvec2 Vadd(register fvec2, register fvec2), Vsub(register fvec2, register fvec2);
fixed Vdist(register fvec2, register fvec2), Vtheta(register fvec2, register fvec2);
Bool Rcollide(frect, frect);
