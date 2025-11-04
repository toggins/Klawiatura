#include "K_os.h"

#ifdef K_OS_WINDOSE

#include <stdio.h>

#include <windows.h>

#endif

void fix_stdio() {
#ifdef K_OS_WINDOSE // source: https://alexanderhoughton.co.uk/blog/redirect-all-stdout-stderr-to-console
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return;

	HANDLE hConOut = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
	SetStdHandle(STD_ERROR_HANDLE, hConOut);

	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif
}

#ifdef K_OS_WINDOSE

static HWND sdl_hwnd = NULL;

HWND get_sdl_hwnd() {
	return sdl_hwnd;
}

// Using a `void*` lets us set `sdl_hwnd` without polluting the global namespace with `windows.h` crap
void set_sdl_hwnd(void* ptr) {
	sdl_hwnd = (HWND)ptr;
}

#endif
