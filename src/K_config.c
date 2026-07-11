#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_file.h"
#include "K_input.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_video.h"

typedef struct {
    const char* name;

    Bool (*r_bool)();
    int (*r_int)();
    float (*r_float)();
    const char* (*r_string)();

    void (*w_bool)(Bool), (*w_int)(int), (*w_float)(float), (*w_string)(const char*);
} ConfigOption;

#define CVAR(cvar, type)                                                                                               \
    static type get_##cvar() {                                                                                         \
        return CLIENT.cvar;                                                                                            \
    }                                                                                                                  \
    static void set_##cvar(type value) {                                                                               \
        CLIENT.cvar = value;                                                                                           \
    }

static const char* get_name() {
    return CLIENT.name;
}
static void set_name(const char* name) {
    SDL_strlcpy(CLIENT.name, (name == NULL || name[0] == '\0') ? DEFAULT_NAME : name, sizeof(CLIENT.name));
}

static const char* get_language() {
    return CLIENT.language;
}
static void set_language(const char* code) {
    SDL_strlcpy(CLIENT.language, (code == NULL || code[0] == '\0') ? DEFAULT_LANGUAGE : code, sizeof(CLIENT.language));
    apply_language(CLIENT.language);
}

CVAR(xscroll, Bool)

CVAR(show_user_messages, Bool)

CVAR(seen_online_notice, Bool)

static const char* get_server() {
    return CLIENT.server;
}
static void set_server(const char* ip) {
    SDL_strlcpy(CLIENT.server, ip, sizeof(CLIENT.server));
    set_hostname(CLIENT.server);
}

CVAR(input_delay, int)

static int cfg_width = 0, cfg_height = 0;
static int get_width() {
    if (!window_maximized())
        get_resolution(&cfg_width, &cfg_height);
    return cfg_width;
}
static void set_width(int width) {
    cfg_width = width;
    if (cfg_height)
        set_resolution(cfg_width, cfg_height, TRUE);
}
static int get_height() {
    if (!window_maximized())
        get_resolution(&cfg_width, &cfg_height);
    return cfg_height;
}
static void set_height(int height) {
    cfg_height = height;
    if (cfg_width)
        set_resolution(cfg_width, cfg_height, TRUE);
}

CVAR(texture_filter, Bool)
CVAR(audio_in_background, Bool)

#undef CVAR
// clang-format off
#define CVAR(name, type) { #name, .r_##type = get_##name, .w_##type = set_##name }
// clang-format on
static const ConfigOption OPTIONS[] = {
    CVAR(name, string),
    CVAR(language, string),
    CVAR(xscroll, bool),
    CVAR(width, int),
    CVAR(height, int),
    CVAR(fullscreen, bool),
    CVAR(framerate, int),
    CVAR(vsync, bool),
    CVAR(texture_filter, bool),
    CVAR(volume, float),
    CVAR(sound_volume, float),
    CVAR(music_volume, float),
    CVAR(audio_in_background, bool),
    CVAR(input_delay, int),
    CVAR(show_user_messages, bool),
    CVAR(server, string),
    CVAR(seen_online_notice, bool),
};
#undef CVAR

void config_init() {
    load_config();
}

void config_teardown() {}

static void parse_kb(yyjson_val* obj, void (*load)(Binding*, yyjson_val*)) {
    yyjson_obj_iter iter = {0};
    yyjson_obj_iter_init(obj, &iter);

    yyjson_val* vkey = NULL;
    while (NULL != (vkey = yyjson_obj_iter_next(&iter))) {
        if (!yyjson_is_str(vkey))
            continue;

        yyjson_val* value = yyjson_obj_iter_get_val(vkey);
        const char* name = yyjson_get_str(vkey);
        for (Keybind i = 0; i < (Keybind)KB_SIZE; i++) {
            Binding* bind = &BINDS[i];
            if (bind->name != NULL && SDL_strcmp(name, bind->name) == 0)
                load(bind, value);
        }
    }
}

static void kb_load_key(Binding* bind, yyjson_val* value) {
    const char* str = yyjson_get_str(yyjson_obj_get(value, "key"));
    if (str != NULL)
        bind->key = SDL_GetScancodeFromName(str);

    str = yyjson_get_str(yyjson_obj_get(value, "button"));
    if (str != NULL)
        bind->button = SDL_GetGamepadButtonFromString(str);

    str = yyjson_get_str(yyjson_obj_get(value, "axis"));
    if (str != NULL)
        bind->axis = SDL_GetGamepadAxisFromString(str);

    yyjson_val* boo = yyjson_obj_get(value, "negative");
    if (yyjson_is_bool(boo))
        bind->negative = yyjson_get_bool(boo);
}

#define HANDLE_OPT(fn, conversion)                                                                                     \
    if (opt->fn != NULL) {                                                                                             \
        opt->fn(conversion(value));                                                                                    \
        goto pc_next_opt;                                                                                              \
    }

static void parse_config(yyjson_val* obj) {
    yyjson_obj_iter iter = {0};
    yyjson_obj_iter_init(obj, &iter);

    yyjson_val* vkey = NULL;
    while (NULL != (vkey = yyjson_obj_iter_next(&iter))) {
        if (!yyjson_is_str(vkey))
            continue;

        yyjson_val* value = yyjson_obj_iter_get_val(vkey);
        const char* name = yyjson_get_str(vkey);
        for (size_t i = 0; i < SDL_arraysize(OPTIONS); i++) {
            const ConfigOption* opt = &OPTIONS[i];
            if (SDL_strcmp(name, opt->name) != 0)
                continue;

            HANDLE_OPT(w_bool, yyjson_get_bool);
            HANDLE_OPT(w_int, yyjson_get_int);
            HANDLE_OPT(w_float, yyjson_get_real);
            HANDLE_OPT(w_string, yyjson_get_str);
        }

    pc_next_opt:
        continue;
    }

    parse_kb(yyjson_obj_get(obj, "controls"), kb_load_key);
}

#undef HANDLE_OPT

void load_config() {
    yyjson_doc* json = load_user_json("config.json");
    if (json == NULL)
        return;

    yyjson_val* root = yyjson_doc_get_root(json);
    if (yyjson_is_obj(root))
        parse_config(root);
    else
        WARN("Config loading skipped, has to be a key-value mapping (got %s)", yyjson_get_type_desc(root));

    yyjson_doc_free(json);
}

static yyjson_mut_val* kb_serialize_key(yyjson_mut_doc* json, const Binding* bind) {
    yyjson_mut_val* obj = yyjson_mut_obj(json);

    yyjson_mut_obj_add(obj, yyjson_mut_str(json, "key"), yyjson_mut_str(json, SDL_GetScancodeName(bind->key)));

    const char* str = SDL_GetGamepadStringForButton(bind->button);
    yyjson_mut_obj_add(obj, yyjson_mut_str(json, "button"), yyjson_mut_str(json, (str == NULL) ? "" : str));

    str = SDL_GetGamepadStringForAxis(bind->axis);
    yyjson_mut_obj_add(obj, yyjson_mut_str(json, "axis"), yyjson_mut_str(json, (str == NULL) ? "" : str));

    yyjson_mut_obj_add(obj, yyjson_mut_str(json, "negative"), yyjson_mut_bool(json, bind->negative));

    return obj;
}

static void save_kb(yyjson_mut_doc* json, yyjson_mut_val* root, const char* name,
    yyjson_mut_val* (*serialize)(yyjson_mut_doc*, const Binding*)) {
    yyjson_mut_val* obj = yyjson_mut_obj(json);
    for (size_t i = 0; i < KB_SIZE; i++) {
        const Binding* binding = &BINDS[i];
        yyjson_mut_obj_add(obj, yyjson_mut_str(json, binding->name), serialize(json, binding));
    }

    yyjson_mut_obj_add(root, yyjson_mut_str(json, name), obj);
}

void save_config() {
    yyjson_mut_doc* json = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(json);
    yyjson_mut_doc_set_root(json, root);

    for (size_t i = 0; i < sizeof(OPTIONS) / sizeof(*OPTIONS); i++) {
        const ConfigOption* opt = &OPTIONS[i];
        yyjson_mut_val *key = yyjson_mut_strcpy(json, opt->name), *value = NULL;

        if (opt->r_bool != NULL)
            value = yyjson_mut_bool(json, opt->r_bool());
        else if (opt->r_int != NULL)
            value = yyjson_mut_sint(json, opt->r_int());
        else if (opt->r_float != NULL)
            value = yyjson_mut_double(json, opt->r_float());
        else if (opt->r_string != NULL)
            value = yyjson_mut_strcpy(json, opt->r_string());
        else
            value = yyjson_mut_null(json);

        yyjson_mut_obj_add(root, key, value);
    }
    save_kb(json, root, "controls", kb_serialize_key);

    size_t size = 0;
    yyjson_write_err error;

    char* buffer = yyjson_mut_write_opts(json, JSON_WRITE_FLAGS, NULL, &size, &error);
    yyjson_mut_doc_free(json);
    ASSUME(buffer, "Failed to save config: %s", error.msg);

    if (save_user_file("config.json", buffer, size))
        INFO("Config saved");
    else
        WARN("Failed to save config");

    SDL_free(buffer);
}
