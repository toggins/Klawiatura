#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define K_OS_WINDOSE
#endif

#ifdef _MSVC_VER
#define K_NORETURN __declspec(noreturn)
#else
#define K_NORETURN __attribute__((noreturn))
#endif

void fix_stdio();
