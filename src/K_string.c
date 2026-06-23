#include "K_string.h"

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
