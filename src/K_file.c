#include <SDL3/SDL_filesystem.h>

#include "K_file.h"

const char* find_file(const char* filename) {
    static char file[256];
    SDL_snprintf(file, sizeof(file), "data/%s.*", filename);

    const char* base = SDL_GetBasePath();
    char** files = SDL_GlobDirectory(base, file, 0, NULL);
    if (files == NULL || files[0] == NULL)
        return NULL;

    SDL_snprintf(file, sizeof(file), "%s%s", base, files[0]);
    SDL_free((void*)files);
    return file;
}
