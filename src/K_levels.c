#include "K_cmd.h"
#include "K_levels.h"
#include "K_log.h"
#include "K_string.h"

static TinyMap levels = {0};
static TinyHash* level_array = NULL;

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

    const TinyHash key = StHashStr(name);
    if (TinyMapGet(&levels, key) == NULL) {
        if (level_array == NULL)
            level_array = MakeTinyDPro(1, sizeof(*level_array));
        level_array = TinyDPush(level_array, &key);
    }
    TinyMapPut(&levels, key, &level, sizeof(level))->cleanup = nuke_level;
}

void levels_init() {
    rediscover_levels();
}

void levels_teardown() {
    FreeTinyMap(&levels);
    FreeTinyD(level_array);
}

void rediscover_levels() {
    FreeTinyMap(&levels);
    FreeTinyD(level_array);
    level_array = NULL;

    iterate_data_files("levels/*", TRUE, iterate_level_file, NULL);
    if (TinyDLength(level_array) > 0)
        SDL_strlcpy(CLIENT.level, get_level_key(level_array[0])->name, sizeof(CLIENT.level));
    else
        CLIENT.level[0] = '\0';
}

const Level* get_level(const char* name) {
    return get_level_key(StHashStr(name));
}

const Level* get_level_key(TinyHash key) {
    return (Level*)TinyMapGet(&levels, key);
}

const char* next_level_from(const char* name) {
    const TinyHash key = StHashStr(name);

    const Level* level = NULL;
    const size_t n = TinyDLength(level_array);
    for (size_t i = 0; i < n; i++) {
        if (key == level_array[i]) {
            level = get_level_key(level_array[(i + 1) % n]);
            break;
        }
    }

    if (level == NULL && n > 0)
        level = get_level_key(level_array[0]);
    return (level == NULL) ? NULL : level->name;
}

const char* last_level_from(const char* name) {
    const TinyHash key = StHashStr(name);

    const Level* level = NULL;
    const size_t n = TinyDLength(level_array);
    for (size_t i = 0; i < n; i++) {
        if (key == level_array[i]) {
            level = get_level_key(level_array[(i > 0) ? (i - 1) : (n - 1)]);
            break;
        }
    }

    if (level == NULL && n > 0)
        level = get_level_key(level_array[0]);
    return (level == NULL) ? NULL : level->name;
}

yyjson_doc* load_level_json(const char* name, const char** err) {
    const Level* level = get_level(name);
    if (level == NULL) {
        if (err != NULL)
            *err = "Level not found";

        return NULL;
    }

    size_t size = 0;
    const void* buffer = load_data_file(fmt("levels/%s.json", level->name), &size);
    if (buffer == NULL) {
        if (err != NULL)
            *err = "Failed to load level file";

        return NULL;
    }

    Uint32 hash = 0;
    for (size_t i = 0; i < size; i++)
        hash += ((Uint8*)buffer)[i];
    if (hash != level->hash) {
        if (err != NULL)
            *err = "Level file was tampered with!";

        SDL_free((void*)buffer);
        return NULL;
    }

    yyjson_doc* json = read_json(buffer, size, err);
    SDL_free((void*)buffer);

    return json;
}
