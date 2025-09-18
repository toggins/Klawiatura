#pragma once

#include <SDL3/SDL_log.h>

#define INFO SDL_Log

#define WARN(fmt, ...)                                                                                                 \
    do {                                                                                                               \
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, fmt, __VA_ARGS__);                                                   \
    } while (0)

#define WTF(fmt, ...)                                                                                                  \
    do {                                                                                                               \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, __VA_ARGS__);                                                  \
    } while (0)

#define FATAL(fmt, ...)                                                                                                \
    do {                                                                                                               \
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, fmt, __VA_ARGS__);                                               \
        exit(1);                                                                                                       \
    } while (0)
