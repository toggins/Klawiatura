#pragma once

#include <SDL3/SDL_log.h>
#include <stdlib.h>

#define INFO(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#define FATAL(...)                                                                                                     \
    do {                                                                                                               \
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__);                                                    \
        exit(EXIT_FAILURE);                                                                                            \
    } while (0)
