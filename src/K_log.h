#pragma once

#include <SDL3/SDL_log.h>

#define INFO SDL_Log

#define WARN(...)                                                                                                      \
    do {                                                                                                               \
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__);                                                        \
    } while (0)

#define WTF(...)                                                                                                       \
    do {                                                                                                               \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__);                                                       \
    } while (0)

#define FATAL(...)                                                                                                     \
    do {                                                                                                               \
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__);                                                    \
        exit(1);                                                                                                       \
    } while (0)
