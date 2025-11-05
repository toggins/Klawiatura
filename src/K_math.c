#include "K_math.h"

/// Add two vectors' components together.
fvec2 Vadd(register fvec2 a, register fvec2 b) {
	a.x += b.x;
	a.y += b.y;
	return a;
}

/// Subtract second vector's components from the first.
fvec2 Vsub(register fvec2 a, register fvec2 b) {
	a.x -= b.x;
	a.y -= b.y;
	return a;
}

/// Approximate distance between two points.
///
/// Pythagorean theorem is useless in fixed point math due to overflows.
fixed Vdist(register fvec2 a, register fvec2 b) {
	const fvec2 d = {Fabs(b.x - a.x), Fabs(b.y - a.y)};
	return (d.x + d.y) - Fhalf(Fmin(d.x, d.y));
}

/// Radian angle between two points.
fixed Vtheta(register fvec2 a, register fvec2 b) {
	b = Vsub(b, a);
	return Fatan2(b.y, b.x);
}

/// Check if there are any collision points between two rectangles.
Bool Rcollide(frect a, frect b) {
	return a.start.x < b.end.x && a.end.x > b.start.x && a.start.y < b.end.y && a.end.y > b.start.y;
}
