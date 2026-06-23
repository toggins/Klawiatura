#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_timer.h>

#include "K_cmake.h"
#include "K_file.h"
#include "K_log.h"
#include "K_string.h"

static const char *base_path = NULL, *user_path = NULL;

static Mod *mods = NULL, *last_mod = NULL;

void file_init(const char** mods, size_t num_mods) {
    base_path = SDL_GetBasePath();
    EXPECT(base_path, "Failed to get base path: %s", SDL_GetError());

    user_path = SDL_GetPrefPath("toggins", GAME_CODENAME);
    EXPECT(user_path, "Failed to get user path: %s", SDL_GetError());

    // Load mods
    if (mods == NULL) {
        load_mod("$MarioForever");
        load_mod("$MarioTogether");
    } else {
        for (size_t i = 0; i < num_mods; i++)
            load_mod(mods[i]);
        SDL_free((void*)mods);
    }
}

void file_teardown() {
    while (mods != NULL) {
        Mod* mod = mods;
        SDL_free((void*)mod->path);
        mods = mod->next;
        SDL_free(mod);
    }

    SDL_free((void*)user_path);
}

void load_mod(const char* path) {
    const size_t plen = SDL_strlen(path);
    if (plen > 0 && path[plen - 1] != '/')
        path = fmt("%s/", path);

    if (path[0] == '$') {
        path =
#ifdef SDL_PLATFORM_EMSCRIPTEN
            fmt("/assets/%s", path + 1);
#else
            fmt("%sdata/%s", base_path, path + 1);
#endif
    }

    SDL_PathInfo pinfo = {0};
    SDL_GetPathInfo(path, &pinfo);
    ASSUME(pinfo.type == SDL_PATHTYPE_DIRECTORY, "Invalid mod \"%s\"", path);

    Mod* mod = SDL_calloc(1, sizeof(*mod));
    EXPECT(mod, "Failed to allocate mod \"%s\"", path);

    mod->path = SDL_strdup(path);
    EXPECT(mod->path, "Failed to allocate path for mod \"%s\": %s", path, SDL_GetError());

    if (mods == NULL)
        mods = mod;
    if (last_mod != NULL)
        last_mod->next = mod;
    mod->previous = last_mod;
    last_mod = mod;

    INFO("Added mod \"%s\"", path);
}

static void* load_file(const char* path, const char* pattern, size_t* size) {
    char** files = SDL_GlobDirectory(path, pattern, 0, NULL);
    if (files == NULL)
        return NULL;

    void* buffer = NULL;
    if (files[0] != NULL) {
        const char* filename = fmt("%s%s", path, files[0]);
        buffer = SDL_LoadFile(filename, size);
        if (buffer == NULL)
            WTF("Failed to load file \"%s\": %s", filename, SDL_GetError());
    }
    SDL_free((void*)files);

    return buffer;
}

static SDL_IOStream* stream_file(const char* path, const char* pattern, Bool write) {
    char** files = SDL_GlobDirectory(path, pattern, 0, NULL);
    if (files == NULL)
        return NULL;

    SDL_IOStream* io = NULL;
    if (files[0] != NULL) {
        const char* filename = fmt("%s%s", path, files[0]);
        io = SDL_IOFromFile(filename, write ? "wb" : "rb");
        if (io == NULL)
            WTF("Failed to stream file \"%s\": %s", filename, SDL_GetError());
    }
    SDL_free((void*)files);

    return io;
}

static yyjson_doc* load_json(const char* path, const char* pattern) {
    char** files = SDL_GlobDirectory(path, pattern, 0, NULL);
    if (files == NULL)
        return NULL;

    yyjson_doc* json = NULL;
    if (files[0] != NULL) {
        const char* filename = fmt("%s%s", path, files[0]);
        yyjson_read_err error = {0};
        json = yyjson_read_file(filename, JSON_READ_FLAGS, NULL, &error);
        if (json == NULL)
            WTF("Failed to load JSON from file \"%s\": %s", filename, error.msg);
    }
    SDL_free((void*)files);

    return json;
}

yyjson_doc* read_json(const char* str, size_t len) {
    yyjson_read_err error = {0};
    yyjson_doc* json = yyjson_read_opts((char*)str, len, JSON_READ_FLAGS, NULL, &error);
    if (json == NULL)
        WTF("Failed to read JSON: %s", error.msg);

    return json;
}

SDL_IOStream* stream_base_file(const char* filename) {
    return stream_file(base_path, filename, FALSE);
}

void* load_data_file(const char* filename, size_t* size) {
    for (const Mod* mod = last_mod; mod != NULL; mod = mod->previous) {
        void* buffer = load_file(mod->path, filename, size);
        if (buffer != NULL)
            return buffer;
    }

    return NULL;
}

SDL_IOStream* stream_data_file(const char* filename) {
    for (const Mod* mod = last_mod; mod != NULL; mod = mod->previous) {
        SDL_IOStream* io = stream_file(mod->path, filename, FALSE);
        if (io != NULL)
            return io;
    }

    return NULL;
}

yyjson_doc* load_data_json(const char* filename) {
    for (const Mod* mod = last_mod; mod != NULL; mod = mod->previous) {
        yyjson_doc* json = load_json(mod->path, filename);
        if (json != NULL)
            return json;
    }

    return NULL;
}

void iterate_data_files(const char* pattern, Bool load_files,
    void (*callback)(const char* filename, void* buffer, size_t size, void* userdata), void* userdata) {
    int count = 0;
    for (const Mod* mod = mods; mod != NULL; mod = mod->next) {
        char** files = SDL_GlobDirectory(mod->path, pattern, 0, &count);
        if (files == NULL)
            continue;

        for (int i = 0; i < count; i++) {
            size_t size = 0;
            void* buffer = load_files ? SDL_LoadFile(fmt("%s%s", mod->path, files[i]), &size) : NULL;
            callback(files[i], buffer, size, userdata);
            SDL_free(buffer);
        }

        SDL_free((void*)files);
    }
}

void* load_user_file(const char* filename, size_t* size) {
    return load_file(user_path, filename, size);
}

yyjson_doc* load_user_json(const char* filename) {
    return load_json(user_path, filename);
}

void iterate_user_files(const char* pattern, Bool load_files,
    void (*callback)(const char* filename, void* buffer, size_t size, void* userdata), void* userdata) {
    int count = 0;
    char** files = SDL_GlobDirectory(user_path, pattern, 0, &count);
    if (files == NULL)
        return;

    for (int i = 0; i < count; i++) {
        size_t size = 0;
        void* buffer = load_files ? SDL_LoadFile(fmt("%s%s", user_path, files[i]), &size) : NULL;
        callback(files[i], buffer, size, userdata);
        SDL_free(buffer);
    }

    SDL_free((void*)files);
}

Bool save_user_file(const char* filename, void* buffer, size_t size) {
    if (!SDL_SaveFile(fmt("%s%s", user_path, filename), buffer, size)) {
        WTF("Failed to save user file \"%s\": %s", filename, SDL_GetError());
        return FALSE;
    }

    return TRUE;
}

Bool save_user_folder(const char* foldername) {
    if (!SDL_CreateDirectory(fmt("%s%s", user_path, foldername))) {
        WTF("Failed to save user folder \"%s\": %s", foldername, SDL_GetError());
        return FALSE;
    }

    return TRUE;
}

// NOLINTBEGIN(misc-no-recursion)
const char* file_basename(const char* path) {
    const char* s = SDL_strrchr(path, '/');
    if (!s)
        s = SDL_strrchr(path, '\\');
    return s ? file_basename(s + 1) : path;
}
// NOLINTEND(misc-no-recursion)

const char* filename_no_ext(const char* path) {
    static char buf[256] = {0};
    SDL_strlcpy(buf, path, sizeof(buf));

    char* s = SDL_strrchr(buf, '.');
    if (s)
        *s = '\0';

    return buf;
}

void read_float_le(SDL_IOStream* io, float* ptr) {
    SDL_ReadIO(io, ptr, sizeof(float));
    *ptr = SDL_SwapFloatLE(*ptr);
}

void read_string(SDL_IOStream* io, char* ptr, size_t size) {
    char c = '\0';
    size_t i = 0;
    do {
        if (SDL_ReadIO(io, &c, sizeof(char)) <= 0) {
            if (i < size)
                ptr[i] = '\0';
            WTF("Reached EOF");
            break;
        }

        if (i < size)
            ptr[i++] = c;
    } while (c != '\0');

    if (i >= size && size > 0)
        ptr[size - 1] = '\0';
}
