#include <stdbool.h>

#include <S_fixed.h>

typedef fix16_t fixed;

typedef struct {
	fixed x, y;
} fvec2;

typedef struct {
	fvec2 start, end;
} frect;

fvec2 Vadd(register fvec2, register fvec2), Vsub(register fvec2, register fvec2);
fixed Vdist(register fvec2, register fvec2), Vtheta(register fvec2, register fvec2);
bool Rcollide(frect, frect);
