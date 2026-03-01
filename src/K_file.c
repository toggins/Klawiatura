#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include "K_file.h"
#include "K_game.h"
#include "K_log.h"
#include "K_misc.h"

static const char* data_path = NULL;
static char* user_path = NULL;

void file_init(const char* path) {
	data_path = path == NULL ? SDL_GetBasePath() : path;
	if (path != NULL)
		INFO("Using data path \"%s\"", path);

	user_path = SDL_GetPrefPath("toggins", GAME_NAME);
	EXPECT(user_path, "SDL_GetPrefPath fail");
}

void file_teardown() {
	SDL_free(user_path);
}

static const char* find_file(const char* base, const char* filename, const char* ignore_ext) {
	int count = 0;
	char** files = SDL_GlobDirectory(base, filename, 0, &count);
	if (files == NULL)
		return NULL;

	static char result[256];
	Bool success = FALSE;
	for (int i = 0; i < count; i++) {
		const char* file = files[i];

		if (ignore_ext != NULL) {
			const char* ext = SDL_strrchr(file, '.');
			if (ext != NULL && SDL_strcmp(ext, ignore_ext) == 0)
				continue;
		}

		SDL_snprintf(result, sizeof(result), "%s%s", base, file);
		success = TRUE;
		break;
	}
	SDL_free((void*)files);

	return success ? result : NULL;
}

/// Finds the first matching "data" file (game assets).
///
/// If not `NULL`, `ignore_ext` will skip any result with the matching file extension.
const char* find_data_file(const char* filename, const char* ignore_ext) {
	return find_file(data_path, filename, ignore_ext);
}

/// Finds the first matching "user" file (config, skins).
///
/// If not `NULL`, `ignore_ext` will skip any result with the matching file extension.
const char* find_user_file(const char* filename, const char* ignore_ext) {
	return find_file(user_path, filename, ignore_ext);
}

const char* get_data_path() {
	return data_path;
}

const char* get_user_path() {
	return user_path;
}

const char* file_basename(const char* path) {
	const char* s = SDL_strrchr(path, '/');
	if (!s)
		s = SDL_strrchr(path, '\\');
	return s ? s + 1 : path;
}

const char* filename_sans_extension(const char* path) {
	static char buf[512] = {0};
	SDL_strlcpy(buf, path, sizeof(buf));

	char* s = SDL_strrchr(buf, '.');
	if (s)
		*s = 0;
	return buf;
}
