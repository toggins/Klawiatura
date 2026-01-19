#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_file.h"
#include "K_input.h"
#include "K_log.h"
#include "K_string.h"
#include "K_video.h"

typedef struct {
	const char* name;

	Bool (*r_bool)();
	int (*r_int)();
	float (*r_float)();
	const char* (*r_string)();

	void (*w_bool)(Bool), (*w_int)(int), (*w_float)(float), (*w_string)(const char*);
} ConfigOption;

static const char* get_name() {
	return CLIENT.user.name;
}
static void set_name(const char* name) {
	SDL_strlcpy(CLIENT.user.name, name, sizeof(CLIENT.user.name));
}
static const char* get_skin() {
	return CLIENT.user.skin;
}
static void set_skin(const char* skin) {
	SDL_strlcpy(CLIENT.user.skin, skin, sizeof(CLIENT.user.skin));
}

static int get_delay() {
	return CLIENT.input.delay;
}
static void set_delay(int delay) {
	CLIENT.input.delay = delay;
}

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
static Bool get_filter() {
	return CLIENT.video.filter;
}
static void set_filter(Bool filter) {
	CLIENT.video.filter = filter;
}
static Bool get_background() {
	return CLIENT.audio.background;
}
static void set_background(Bool background) {
	CLIENT.audio.background = background;
}
static Bool get_aware() {
	return CLIENT.user.aware;
}
static void set_aware(Bool aware) {
	CLIENT.user.aware = aware;
}

static const ConfigOption OPTIONS[] = {
	{"aware",        .r_bool = get_aware,         .w_bool = set_aware        },
	{"name",         .r_string = get_name,        .w_string = set_name       },
	{"skin",         .r_string = get_skin,        .w_string = set_skin       },
	{"delay",        .r_int = get_delay,          .w_int = set_delay         },
	{"width",        .r_int = get_width,          .w_int = set_width         },
	{"height",       .r_int = get_height,         .w_int = set_height        },
	{"fullscreen",   .r_bool = get_fullscreen,    .w_bool = set_fullscreen   },
	{"framerate",    .r_int = get_framerate,      .w_int = set_framerate     },
	{"vsync",        .r_bool = get_vsync,         .w_bool = set_vsync        },
	{"filter",       .r_bool = get_filter,        .w_bool = set_filter       },
	{"volume",       .r_float = get_volume,       .w_float = set_volume      },
	{"sound_volume", .r_float = get_sound_volume, .w_float = set_sound_volume},
	{"music_volume", .r_float = get_music_volume, .w_float = set_music_volume},
	{"background",   .r_bool = get_background,    .w_bool = set_background   },
};

static Bool fill_missing_fields() {
	int modified = 0;

	if (!SDL_strlen(CLIENT.user.name))
		SDL_snprintf(CLIENT.user.name, CLIENT_STRING_MAX, "Mario #%04d", 1 + SDL_rand(9999)), modified++;

	if (!SDL_strlen(CLIENT.user.skin))
		SDL_snprintf(CLIENT.user.skin, CLIENT_STRING_MAX, "mario"), modified++;

	if (modified)
		INFO("Set %d options to their default values", modified);
	return modified > 0;
}

static const char* config_path = NULL;

void config_init(const char* path) {
	config_path = path;
	if (config_path != NULL)
		INFO("Using config \"%s\"", config_path);
	load_config();
}

void config_teardown() {}

static void parse_kb(yyjson_val* obj, void (*load)(Bindings*, yyjson_val*)) {
	if (obj == NULL)
		return;

	yyjson_obj_iter iter = {0};
	yyjson_obj_iter_init(obj, &iter);

	yyjson_val* vkey = NULL;
	while (NULL != (vkey = yyjson_obj_iter_next(&iter))) {
		if (!yyjson_is_str(vkey))
			continue;
		yyjson_val* value = yyjson_obj_iter_get_val(vkey);
		const char* name = yyjson_get_str(vkey);
		for (Keybind i = 0; i < (Keybind)KB_SIZE; i++) {
			Bindings* bind = &BINDS[i];
			if (bind->name != NULL && !SDL_strcmp(name, bind->name))
				load(bind, value);
		}
	}
}

static void kb_load_key(Bindings* bind, yyjson_val* value) {
	bind->key = yyjson_get_int(yyjson_obj_get(value, "key"));
	bind->button = yyjson_get_int(yyjson_obj_get(value, "button"));
	bind->axis = yyjson_get_int(yyjson_obj_get(value, "axis"));
	bind->negative = yyjson_get_bool(yyjson_obj_get(value, "negative"));
}

#define HANDLE_OPT(fn, conversion)                                                                                     \
	if (opt->fn != NULL) {                                                                                         \
		opt->fn(conversion(value));                                                                            \
		goto next_opt;                                                                                         \
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
		for (int i = 0; i < sizeof(OPTIONS) / sizeof(*OPTIONS); i++) {
			const ConfigOption* opt = &OPTIONS[i];
			if (SDL_strcmp(name, opt->name))
				continue;
			HANDLE_OPT(w_bool, yyjson_get_bool);
			HANDLE_OPT(w_int, yyjson_get_int);
			HANDLE_OPT(w_float, yyjson_get_real);
			HANDLE_OPT(w_string, yyjson_get_str);
		}
	next_opt:
		continue;
	}

	parse_kb(yyjson_obj_get(obj, "kbd"), kb_load_key);
}
#undef HANDLE_OPT

void load_config() {
	const char* config_file = config_path == NULL ? find_user_file("config.json", NULL) : config_path;
	if (!config_file) {
		WARN("No config to load; writing a default one...");
		goto overwrite;
	}

	yyjson_read_err error;
	yyjson_doc* json = yyjson_read_file(config_file, JSON_READ_FLAGS, NULL, &error);
	ASSUME(json, "Failed to load config: %s", error.msg);

	yyjson_val* root = yyjson_doc_get_root(json);
	if (yyjson_is_obj(root))
		parse_config(root);
	else
		WARN("Config loading skipped, has to be a key-value mapping (got %s)", yyjson_get_type_desc(root));
	yyjson_doc_free(json);

overwrite:
	if (fill_missing_fields())
		save_config();
}

static yyjson_mut_val* kb_serialize_key(yyjson_mut_doc* json, const Bindings* bind) {
	yyjson_mut_val* obj = yyjson_mut_obj(json);
	yyjson_mut_obj_add(obj, yyjson_mut_str(json, "key"), yyjson_mut_int(json, bind->key));
	yyjson_mut_obj_add(obj, yyjson_mut_str(json, "button"), yyjson_mut_int(json, bind->button));
	yyjson_mut_obj_add(obj, yyjson_mut_str(json, "axis"), yyjson_mut_int(json, bind->axis));
	yyjson_mut_obj_add(obj, yyjson_mut_str(json, "negative"), yyjson_mut_int(json, bind->negative));
	return obj;
}

static void save_kb(yyjson_mut_doc* json, yyjson_mut_val* root, const char* name,
	yyjson_mut_val* (*serialize)(yyjson_mut_doc*, const Bindings*)) {
	yyjson_mut_val* obj = yyjson_mut_obj(json);

	for (Keybind i = 0; i < (Keybind)KB_SIZE; i++) {
		const Bindings* binding = &BINDS[i];
		yyjson_mut_val* key = yyjson_mut_str(json, binding->name);
		yyjson_mut_obj_add(obj, key, serialize(json, binding));
	}

	yyjson_mut_val* key = yyjson_mut_str(json, name);
	yyjson_mut_obj_add(root, key, obj);
}

void save_config() {
	fill_missing_fields(); // fill missing after quitting the settings menu

	WHATEVER(!config_path, "Saving config outside of user path is explicitly prohibited");
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
	save_kb(json, root, "kbd", kb_serialize_key);

	size_t size = 0;
	yyjson_write_err error;

	char* buffer = yyjson_mut_write_opts(json, JSON_WRITE_FLAGS, NULL, &size, &error);
	yyjson_mut_doc_free(json);
	ASSUME(buffer, "Failed to save config: %s", error.msg);

	const char* path = fmt("%sconfig.json", get_user_path());
	if (SDL_SaveFile(path, buffer, size))
		INFO("Config saved");
	else
		WARN("Failed to save config: %s", SDL_GetError());
	SDL_free(buffer);
}
