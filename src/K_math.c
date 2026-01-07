#include "K_math.h"

/// Add two vectors' components together.
FVec2 Vadd(register FVec2 a, register FVec2 b) {
	a.x += b.x, a.y += b.y;
	return a;
}

/// Subtract second vector's components from the first.
FVec2 Vsub(register FVec2 a, register FVec2 b) {
	a.x -= b.x, a.y -= b.y;
	return a;
}

/// Scale a vector's components by a scalar.
FVec2 Vscale(register FVec2 v, register Fixed s) {
	v.x = Fmul(v.x, s), v.y = Fmul(v.y, s);
	return v;
}

/// Approximate distance between two points.
///
/// Pythagorean theorem is useless in Fixed point math due to overflows.
Fixed Vdist(register FVec2 a, register FVec2 b) {
	const FVec2 d = {Fabs(b.x - a.x), Fabs(b.y - a.y)};
	return (d.x + d.y) - Fhalf(Fmin(d.x, d.y));
}

/// Radian angle between two points.
Fixed Vtheta(register FVec2 a, register FVec2 b) {
	b = Vsub(b, a);
	return Fatan2(b.y, b.x);
}

/// Check if there are any collision points between two rectangles.
Bool Rcollide(FRect a, FRect b) {
	return a.start.x < b.end.x && a.end.x > b.start.x && a.start.y < b.end.y && a.end.y > b.start.y;
}
