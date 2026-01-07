#include <stdlib.h>
// ^ required for `exit(EXIT_FAILURE)` below. DO NOT TOUCH YOU FUCKER

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>

#include "K_file.h"
#include "K_log.h"
#include "K_string.h"

void handle_fatal(const char* file, int line, const char* func, const char* fmt, ...) {
	static char buf[512] = "";
	extern SDL_Window* window;

	static SDL_MessageBoxButtonData butts[1] = {0};
	butts[0].text = "Damn it";
	butts[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

	SDL_MessageBoxData data = {0};
	data.window = window, data.message = buf;
	data.title = "Klawiatura FATAL ERROR", data.flags = SDL_MESSAGEBOX_ERROR;
	data.buttons = butts, data.numbuttons = sizeof(butts) / sizeof(*butts);

	va_list args = {0};
	va_start(args, fmt);
	SDL_snprintf(
		buf, sizeof(buf), "File: %s:%d\nFunction: %s()\n\n%s", log_basename(file), line, func, vfmt(fmt, args));
	va_end(args);

	SDL_ShowMessageBox(&data, NULL);
	exit(EXIT_FAILURE);
}

const char* log_basename(const char* path) {
	return file_basename(path);
}

static const char* log_levels[SDL_LOG_PRIORITY_COUNT] = {
	[SDL_LOG_PRIORITY_INFO] = "INFO",
	[SDL_LOG_PRIORITY_WARN] = "WARN",
	[SDL_LOG_PRIORITY_ERROR] = "ERROR",
	[SDL_LOG_PRIORITY_CRITICAL] = "FATAL",
};

static void klaw_logger(void* userdata, int category, SDL_LogPriority priority, const char* message) {
	fprintf(stderr, "[%s] %s\n", log_levels[priority], message);
	fflush(stderr);
}

void log_init() {
	SDL_SetLogOutputFunction(klaw_logger, NULL);
}
