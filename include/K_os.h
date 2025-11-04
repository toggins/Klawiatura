#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define K_OS_WINDOSE
#endif

void fix_stdio();
