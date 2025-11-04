#include <stdlib.h>
// ^ required for `exit(EXIT_FAILURE)` below. DO NOT TOUCH YOU FUCKER

#include "K_file.h"
#include "K_log.h"
#include "K_os.h"
#include "K_string.h"

#ifdef K_OS_WINDOSE
#include <windows.h>
#endif

void handle_fatal(const char* fmt, ...) {
#ifdef K_OS_WINDOSE
	extern HWND get_sdl_hwnd();

	MSGBOXPARAMS msg = {0};
	msg.cbSize = sizeof(msg), msg.hwndOwner = get_sdl_hwnd();
	msg.dwStyle = MB_ICONERROR | MB_OK | MB_SYSTEMMODAL;
	msg.dwLanguageId = LANG_ENGLISH;
	msg.lpszCaption = "Klawiatura FATAL ERROR";

	va_list args;
	va_start(args, fmt);
	msg.lpszText = vfmt(fmt, args);
	va_end(args);

	MessageBoxIndirect(&msg);
#endif

	exit(EXIT_FAILURE);
}

const char* log_basename(const char* path) {
	return file_basename(path);
}

static const char* log_levels[SDL_LOG_PRIORITY_COUNT] = {
	[SDL_LOG_PRIORITY_INFO] = "INFO",
	[SDL_LOG_PRIORITY_WARN] = "WARN",
	[SDL_LOG_PRIORITY_ERROR] = "ERROR",
	[SDL_LOG_PRIORITY_CRITICAL] = "SHIT",
};

static void klaw_logger(void* userdata, int category, SDL_LogPriority priority, const char* message) {
	fprintf(stderr, "[%s] %s\n", log_levels[priority], message);
	fflush(stderr);
}

void log_init() {
	SDL_SetLogOutputFunction(klaw_logger, NULL);
}
