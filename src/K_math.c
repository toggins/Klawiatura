#include "K_math.h" // IWYU pragma: keep

void fvec2_zero(fvec2 dest) {
    dest[0] = FxZero;
    dest[1] = FxZero;
}

void fvec2_copy(const fvec2 src, fvec2 dest) {
    dest[0] = src[0];
    dest[1] = src[1];
}

void fvec2_lerp(const fvec2 a, const fvec2 b, fix16_t x, fvec2 dest) {
    fix16_t x1 = a[0];
    fix16_t y1 = a[1];
    fix16_t x2 = b[0];
    fix16_t y2 = b[1];
    dest[0] = Flerp(x1, x2, x);
    dest[1] = Flerp(y1, y2, x);
}

bool fvec2_equal(const fvec2 a, const fvec2 b) {
    return a[0] == b[0] && a[1] == b[1];
}
