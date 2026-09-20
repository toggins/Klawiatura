#include "K_string.h"
#include "K_tick.h"

#define FMT_SIZE 1024
#define FMT_PADDING 256

const char* vfmt(const char* pattern, va_list args) {
    static char buf[FMT_SIZE + FMT_PADDING] = {0};
    static char* cur = buf;

    size_t remaining = sizeof(buf);
    const size_t ahead = cur - buf;
    if (ahead < FMT_SIZE)
        remaining -= ahead;
    else
        cur = buf;

    char* dest = cur;
    const int written = SDL_vsnprintf(dest, remaining, pattern, args);
    if (written < 0)
        return "";

    cur += (written < remaining) ? (written + 1) : remaining;

    return dest;
}

const char* fmt(const char* pattern, ...) {
    va_list args = {0};
    va_start(args, pattern);
    const char* res = vfmt(pattern, args);
    va_end(args);

    return res;
}

const char* caret(Bool active) {
    if (!active)
        return "";

    const float tickrate = (float)get_tickrate();
    return (SDL_fmodf(uiticks(), tickrate) < (tickrate * 0.5f)) ? "|" : " ";
}

const char* u64_to_base32(Uint64 num) {
    static const char base[32] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    static char out[BASE32_STRING_SIZE] = "";

    if (num <= 0) {
        out[0] = base[0];
        out[1] = '\0';

        return out;
    }

    char* i = out + sizeof(out) - 1;
    *i = '\0';
    while (num > 0) {
        *(--i) = base[num % sizeof(base)];
        num /= sizeof(base);
    }

    return i;
}

Uint64 base32_to_u64(const char* str) {
    static const Uint8 table[256] = {
        ['A'] = 0,
        ['B'] = 1,
        ['C'] = 2,
        ['D'] = 3,
        ['E'] = 4,
        ['F'] = 5,
        ['G'] = 6,
        ['H'] = 7,
        ['I'] = 8,
        ['J'] = 9,
        ['K'] = 10,
        ['L'] = 11,
        ['M'] = 12,
        ['N'] = 13,
        ['O'] = 14,
        ['P'] = 15,
        ['Q'] = 16,
        ['R'] = 17,
        ['S'] = 18,
        ['T'] = 19,
        ['U'] = 20,
        ['V'] = 21,
        ['W'] = 22,
        ['X'] = 23,
        ['Y'] = 24,
        ['Z'] = 25,
        ['2'] = 26,
        ['3'] = 27,
        ['4'] = 28,
        ['5'] = 29,
        ['6'] = 30,
        ['7'] = 31,

        ['a'] = 0,
        ['b'] = 1,
        ['c'] = 2,
        ['d'] = 3,
        ['e'] = 4,
        ['f'] = 5,
        ['g'] = 6,
        ['h'] = 7,
        ['i'] = 8,
        ['j'] = 9,
        ['k'] = 10,
        ['l'] = 11,
        ['m'] = 12,
        ['n'] = 13,
        ['o'] = 14,
        ['p'] = 15,
        ['q'] = 16,
        ['r'] = 17,
        ['s'] = 18,
        ['t'] = 19,
        ['u'] = 20,
        ['v'] = 21,
        ['w'] = 22,
        ['x'] = 23,
        ['y'] = 24,
        ['z'] = 25,
    };

    if (str == NULL || str[0] == '\0')
        return 0;

    Uint64 result = 0;
    while (*str) {
        const Uint8 val = table[*str];
        if (val > 31)
            break;

        result = (result << 5) | val;
        ++str;
    }

    return result;
}
