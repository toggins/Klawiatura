#pragma once

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

__attribute__((noreturn)) void DIE();
const char* log_basename(const char*);
#define LOG_WITH(fn, msg, ...)                                                                                         \
	fn(SDL_LOG_CATEGORY_APPLICATION, "%s:%d %s(): " msg, log_basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__)

#define INFO(...) LOG_WITH(SDL_LogInfo, __VA_ARGS__)
#define WARN(...) LOG_WITH(SDL_LogWarn, __VA_ARGS__)
#define WTF(...) LOG_WITH(SDL_LogError, __VA_ARGS__)
#define FATAL(...) (LOG_WITH(SDL_LogCritical, __VA_ARGS__), DIE())
#define EXPECT(expr, ...)                                                                                              \
	do {                                                                                                           \
		if (!(expr))                                                                                           \
			FATAL(__VA_ARGS__);                                                                            \
	} while (0)
#define ASSUME(expr, ...)                                                                                              \
	do {                                                                                                           \
		if (!(expr)) {                                                                                         \
			WTF(__VA_ARGS__);                                                                              \
			return;                                                                                        \
		}                                                                                                      \
	} while (0)
