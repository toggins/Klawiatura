#include "K_file.h"
#include "K_levels.h"
#include "K_log.h"

static TinyMap levels = {0};

static void nuke_level(void* ptr) {
    Level* level = ptr;
    SDL_free((void*)level->name);
}

static void iterate_level_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    (void)userdata;

    const char* error = NULL;
    yyjson_doc* json = read_json(buffer, size, &error);
    ASSUME(json, "Failed to read level \"%s\": %s", filename, error);

    const char* name = filename_no_ext(file_basename(filename));

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        WTF("Expected level \"%s\" JSON root as object, got %s", name, yyjson_get_type_desc(root));
        yyjson_doc_free(json);
        return;
    }

    Level level = {0};

    level.name = SDL_strdup(name);
    EXPECT(level.name, "Failed to allocate level \"%s\" name", name);

    for (size_t i = 0; i < size; i++)
        level.hash += ((Uint8*)buffer)[i];

    yyjson_doc_free(json);

    TinyDictPut(&levels, name, &level, sizeof(level))->cleanup = nuke_level;
}

void levels_init() {
    iterate_data_files("levels/*", TRUE, iterate_level_file, NULL);
}

void levels_teardown() {
    FreeTinyMap(&levels);
}

const Level* get_level(const char* name) {
    return get_level_key(StHashStr(name));
}

const Level* get_level_key(TinyHash key) {
    return (Level*)TinyMapGet(&levels, key);
}
