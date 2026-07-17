#include "K_math.h"

/// Add two vectors' components together.
FVec2 Vadd(FVec2 a, FVec2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

/// Subtract second vector's components from the first.
FVec2 Vsub(FVec2 a, FVec2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

/// Multiply the first vector by the second vector.
FVec2 Vmul(FVec2 a, FVec2 b) {
    a.x = Fmul(a.x, b.x);
    a.y = Fmul(a.y, b.y);
    return a;
}

/// Divide the first vector by the second vector.
FVec2 Vdiv(FVec2 a, FVec2 b) {
    a.x = Fdiv(a.x, b.x);
    a.y = Fdiv(a.y, b.y);
    return a;
}

/// Scale a vector's components by a scalar.
FVec2 Vscale(FVec2 v, Fixed s) {
    v.x = Fmul(v.x, s);
    v.y = Fmul(v.y, s);
    return v;
}

/// Approximate distance between two points.
///
/// Pythagorean theorem is useless in Fixed point math due to overflows.
Fixed Vdist(FVec2 a, FVec2 b) {
    const FVec2 d = {Fabs(b.x - a.x), Fabs(b.y - a.y)};
    return (d.x + d.y) - Fhalf(Fmin(d.x, d.y));
}

/// Radian angle between two points.
Fixed Vtheta(FVec2 a, FVec2 b) {
    b = Vsub(b, a);
    return Fatan2(b.y, b.x);
}

/// Lerp between two points.
FVec2 Vlerp(FVec2 a, FVec2 b, Fixed c) {
    a.x = Flerp(a.x, b.x, c);
    a.y = Flerp(a.y, b.y, c);
    return a;
}

/// Clamp between two points.
FVec2 Vclamp(FVec2 a, FVec2 b, FVec2 c) {
    a.x = Fclamp(a.x, b.x, c.x);
    a.y = Fclamp(a.y, b.y, c.y);
    return a;
}

/// Add a rectangle by a vector.
FRect Radd(FRect a, FVec2 b) {
    a.start.x += b.x;
    a.start.y += b.y;
    a.end.x += b.x;
    a.end.y += b.y;
    return a;
}

/// Multiply a rectangle by a vector.
FRect Rmul(FRect a, FVec2 b) {
    const Fixed x1 = a.start.x, y1 = a.start.y, x2 = a.end.x, y2 = a.end.y;

    if (b.x >= Fx0) {
        a.start.x = Fmul(x1, b.x);
        a.end.x = Fmul(x2, b.x);
    } else {
        a.start.x = Fmul(-x2, b.x);
        a.end.x = Fmul(-x1, b.x);
    }

    if (b.y >= Fx0) {
        a.start.y = Fmul(y1, b.y);
        a.end.y = Fmul(y2, b.y);
    } else {
        a.start.y = Fmul(-y2, b.y);
        a.end.y = Fmul(-y1, b.y);
    }

    return a;
}

/// Flip a rectangle along the X-axis.
FRect Rxflip(FRect a) {
    const Fixed x1 = a.start.x, x2 = a.end.x;
    a.start.x = -x2;
    a.end.x = -x1;
    return a;
}

/// Flip a rectangle along the Y-axis.
FRect Ryflip(FRect a) {
    const Fixed y1 = a.start.y, y2 = a.end.y;
    a.start.y = -y2;
    a.end.y = -y1;
    return a;
}

FVec2 Rcenter(FRect a) {
    return Vlerp(a.start, a.end, FxHalf);
}

/// Check if there are any collision points between two rectangles.
Bool Rcollide(FRect a, FRect b) {
    return a.start.x < b.end.x && a.end.x > b.start.x && a.start.y < b.end.y && a.end.y > b.start.y;
}
