#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include "K_file.h"

const char* file_pattern(const char* fmt, ...) {
	static char pattern_helper[256];

	va_list args;
	va_start(args, fmt);
	SDL_vsnprintf(pattern_helper, sizeof(pattern_helper), fmt, args);
	va_end(args);

	return pattern_helper;
}

const char* find_file(const char* filename, const char* ignore_ext) {
	const char* base = SDL_GetBasePath();

	int count = 0;
	char** files = SDL_GlobDirectory(base, filename, 0, &count);
	if (files == NULL)
		return NULL;

	static char result[256];
	bool success = false;
	for (int i = 0; i < count; i++) {
		const char* file = files[i];

		if (ignore_ext != NULL) {
			const char* ext = SDL_strrchr(file, '.');
			if (ext != NULL && SDL_strcmp(ext, ignore_ext) == 0)
				continue;
		}

		SDL_snprintf(result, sizeof(result), "%s%s", base, file);
		success = true;
		break;
	}
	SDL_free((void*)files);

	return success ? result : NULL;
}
