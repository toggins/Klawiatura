#pragma once

#include <stdlib.h> // IWYU pragma: keep
// ^ required for `exit(EXIT_FAILURE)` below. DO NOT TOUCH YOU FUCKER

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#define LOG_WITH(fn, ...) fn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#define INFO(...) LOG_WITH(SDL_LogInfo, __VA_ARGS__)
#define WARN(...) LOG_WITH(SDL_LogWarn, __VA_ARGS__)
#define WTF(...) LOG_WITH(SDL_LogError, __VA_ARGS__)
#define FATAL(...)                                                                                                     \
	do {                                                                                                           \
		LOG_WITH(SDL_LogCritical, __VA_ARGS__);                                                                \
		exit(EXIT_FAILURE);                                                                                    \
	} while (0)
