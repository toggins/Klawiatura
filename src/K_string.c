#include <SDL3/SDL_stdinc.h>

#include "K_string.h"

const char* vfmt(const char* fmt_pattern, va_list args) {
	static char buf[1024] = {0};
	SDL_vsnprintf(buf, sizeof(buf), fmt_pattern, args);
	return buf;
}

const char* fmt(const char* fmt_pattern, ...) {
	va_list args = {0};
	va_start(args, fmt_pattern);
	const char* res = vfmt(fmt_pattern, args);
	va_end(args);
	return res;
}
