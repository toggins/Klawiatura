#pragma once

#include <SDL3/SDL_stdinc.h>

const char *fmt(const char*, ...), *vfmt(const char*, va_list);
const char* txnum(uint64_t);
