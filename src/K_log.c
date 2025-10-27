#include <stdlib.h>
// ^ required for `exit(EXIT_FAILURE)` below. DO NOT TOUCH YOU FUCKER

#include "K_file.h"
#include "K_log.h"

const char* log_basename(const char* path) {
	return file_basename(path);
}

void DIE() {
	exit(EXIT_FAILURE);
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
