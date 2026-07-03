#include "K_cmd.h"
#include "K_file.h"
#include "K_locale.h"
#include "K_log.h"

#define MAX_LFMT_ARGS 8

static TinyMap languages = {0};
static Language *default_language = NULL, *current_language = NULL;

static TinyMapIterator iterator = {0};

static void nuke_language(void* ptr) {
    Language* language = ptr;
    SDL_free((void*)language->name);
    FreeTinyMap(&language->strings);
}

static void iterate_language_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    yyjson_doc* json = read_json(buffer, size, NULL);
    ASSUME(json, "Failed to read language \"%s\"", filename);

    yyjson_val* root = yyjson_doc_get_root(json);
    if (yyjson_is_obj(root)) {
        const char* name = filename_no_ext(file_basename(filename));
        const TinyHash hash = StHashStr(name);

        Language* language = (Language*)TinyMapGet(&languages, hash);
        if (language == NULL) {
            Language lang = {0};
            lang.name = SDL_strdup(name);
            EXPECT(lang.name, "Failed to allocate name for language \"%s\"", name);

            TinyBucket* bucket = TinyMapPut(&languages, hash, &lang, sizeof(lang));
            bucket->cleanup = nuke_language;
            language = bucket->data;
        }

        size_t i = 0, n = 0;
        yyjson_val *key = NULL, *val = NULL;
        yyjson_obj_foreach(root, i, n, key, val) {
            const char *sk = yyjson_get_str(key), *sv = yyjson_get_str(val);
            if (sk == NULL || sv == NULL)
                continue;

            TinyDictPut(&language->strings, sk, sv, SDL_strlen(sv) + 1);
        }

        Bool* found_default = userdata;
        if (!*found_default && SDL_strcmp(language->name, DEFAULT_LANGUAGE) == 0) {
            default_language = language;
            *found_default = TRUE;
        }
    }

    yyjson_doc_free(json);
}

void locale_init() {
    Bool found_default = FALSE;
    iterate_data_files("languages/*", TRUE, iterate_language_file, &found_default);
}

void locale_teardown() {
    FreeTinyMap(&languages);
}

void apply_language(const char* name) {
    current_language = (Language*)TinyDictGet(&languages, name);
    WHATEVER(current_language, "Language \"%s\" not found", name);
    INFO("Applied language \"%s\"", current_language->name);
}

static const char* localized(const char* key) {
    const char* str = NULL;

    const TinyHash hash = StHashStr(key);
    if (current_language != NULL) {
        str = TinyMapGet(&current_language->strings, hash);
        if (str != NULL)
            return str;
    }
    if (default_language != NULL)
        str = TinyMapGet(&default_language->strings, hash);

    return (str == NULL) ? key : str;
}

const char* handle_lfmt(const char* key, ...) {
    static char buf[1024], abuf[1024];

    const char* template = localized(key);
    if (template == NULL)
        return NULL;

    // Parse arguments
    va_list args = {0};
    va_start(args, key);

    size_t num_args = 0;
    const char* rargs[MAX_LFMT_ARGS] = {0};
    char* rpos = abuf;

    while (num_args < MAX_LFMT_ARGS) {
        const char type = va_arg(args, int);
        if (type == '\0')
            goto lfmt_end_parse;

        const size_t remaining = (abuf + sizeof(abuf)) - rpos;
        if (remaining <= 1)
            break;

        int written = 0;
        switch (type) {
        default:
            goto lfmt_end_parse;

        case 'd': {
            const int val = va_arg(args, int);
            written = SDL_snprintf(rpos, remaining, "%d", val);
            break;
        }

        case 's': {
            const char* val = va_arg(args, const char*);
            written = SDL_snprintf(rpos, remaining, "%s", val);
            break;
        }
        }

        if (written < 0 || written >= remaining)
            break;

        rargs[num_args++] = rpos;
        rpos += written + 1;
    }

lfmt_end_parse:
    va_end(args);

    if (num_args <= 0)
        return template;

    // Interpolate string
    const char* in = template;
    char* out = buf;
    const char* end = buf + sizeof(buf) - 1;

    while (*in != '\0') {
        if (*in == '{') {
            if (*(in + 1) == '{') {
                if (out >= end)
                    break;

                *out++ = '{';
                in += 2;
                continue;
            }

            const char* brace = SDL_strchr(in, '}');
            if (brace != NULL) {
                const int idx = SDL_atoi(in + 1);
                if (idx >= 0 && idx < num_args) {
                    const size_t remaining = end - out;
                    const size_t copied = SDL_snprintf(out, remaining + 1, "%s", rargs[idx]);
                    out += (copied > remaining) ? remaining : copied;
                }

                in = brace + 1;
                continue;
            }
        }

        if (out >= end)
            break;
        *out++ = *in++;
    }

    *out = '\0';
    return buf;
}

void language_iterate_start() {
    iterator = TinyMapIter(&languages);
}

const Language* language_iterate_next() {
    return TinyMapNext(&iterator) ? iterator.data : NULL;
}
