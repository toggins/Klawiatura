#include <SDL3/SDL_stdinc.h>

#include "K_string.h"

const char* fmt(const char* fmt_pattern, ...) {
	static char buf[1024] = {0};
	va_list args;

	va_start(args, fmt_pattern);
	SDL_vsnprintf(buf, sizeof(buf), fmt_pattern, args);
	va_end(args);

	return buf;
}
