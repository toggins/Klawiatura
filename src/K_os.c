#include <SDL3/SDL_platform_defines.h>

#ifdef SDL_PLATFORM_WIN32
#include <stdio.h>

#include <windows.h>
#endif

#include "K_os.h"

void fix_stdio() {
#ifdef SDL_PLATFORM_WIN32 // source: https://alexanderhoughton.co.uk/blog/redirect-all-stdout-stderr-to-console
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;

    HANDLE hConOut = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
    SetStdHandle(STD_ERROR_HANDLE, hConOut);

    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif
}
