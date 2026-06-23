#include "K_math.h"

/// Add two vectors' components together.
FVec2 Vadd(register FVec2 a, register FVec2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

/// Subtract second vector's components from the first.
FVec2 Vsub(register FVec2 a, register FVec2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

/// Multiply the first vector by the second vector.
FVec2 Vmul(register FVec2 a, register FVec2 b) {
    a.x = Fmul(a.x, b.x);
    a.y = Fmul(a.y, b.y);
    return a;
}

/// Divide the first vector by the second vector.
FVec2 Vdiv(register FVec2 a, register FVec2 b) {
    a.x = Fdiv(a.x, b.x);
    a.y = Fdiv(a.y, b.y);
    return a;
}

/// Scale a vector's components by a scalar.
FVec2 Vscale(register FVec2 v, register Fixed s) {
    v.x = Fmul(v.x, s);
    v.y = Fmul(v.y, s);
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

/// Lerp between two points.
FVec2 Vlerp(register FVec2 a, register FVec2 b, register Fixed c) {
    a.x = Flerp(a.x, b.x, c);
    a.y = Flerp(a.y, b.y, c);
    return a;
}

/// Add a rectangle by a vector.
FRect Radd(register FRect a, register FVec2 b) {
    a.start.x += b.x;
    a.start.y += b.y;
    a.end.x += b.x;
    a.end.y += b.y;
    return a;
}

/// Flip a rectangle along the X-axis.
FRect Rxflip(register FRect a) {
    const register Fixed x1 = a.start.x, x2 = a.end.x;
    a.start.x = -x2;
    a.end.x = -x1;
    return a;
}

/// Flip a rectangle along the Y-axis.
FRect Ryflip(register FRect a) {
    const register Fixed y1 = a.start.y, y2 = a.end.y;
    a.start.y = -y2;
    a.end.y = -y1;
    return a;
}

FVec2 Rcenter(register FRect a) {
    return Vlerp(a.start, a.end, FxHalf);
}

/// Check if there are any collision points between two rectangles.
Bool Rcollide(register FRect a, register FRect b) {
    return a.start.x < b.end.x && a.end.x > b.start.x && a.start.y < b.end.y && a.end.y > b.start.y;
}
