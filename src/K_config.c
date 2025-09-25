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

	bool (*r_bool)();
	int (*r_int)();
	float (*r_float)();
	const char* (*r_string)();

	void (*w_bool)(bool);
	void (*w_int)(int);
	void (*w_float)(float);
	void (*w_string)(const char*);
} ConfigOption;

extern ClientInfo CLIENT;
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

static int cfg_width = 0, cfg_height = 0;
static int get_width() {
	if (!get_fullscreen())
		get_resolution(&cfg_width, &cfg_height);
	return cfg_width;
}
static void set_width(int width) {
	cfg_width = width;
	if (cfg_height)
		set_resolution(cfg_width, cfg_height);
}
static int get_height() {
	if (!get_fullscreen())
		get_resolution(&cfg_width, &cfg_height);
	return cfg_height;
}
static void set_height(int height) {
	cfg_height = height;
	if (cfg_width)
		set_resolution(cfg_width, cfg_height);
}

static const ConfigOption OPTIONS[] = {
	{"name",         .r_string = get_name,        .w_string = set_name       },
	{"skin",         .r_string = get_skin,        .w_string = set_skin       },
	{"width",        .r_int = get_width,          .w_int = set_width         },
	{"height",       .r_int = get_height,         .w_int = set_height        },
	{"fullscreen",   .r_bool = get_fullscreen,    .w_bool = set_fullscreen   },
	{"vsync",        .r_bool = get_vsync,         .w_bool = set_vsync        },
	{"volume",       .r_float = get_volume,       .w_float = set_volume      },
	{"sound_volume", .r_float = get_sound_volume, .w_float = set_sound_volume},
	{"music_volume", .r_float = get_music_volume, .w_float = set_music_volume},
};

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
		for (int i = 0; i < KB_SIZE; i++) {
			Bindings* bind = &BINDS[i];
			if (bind->name != NULL && !SDL_strcmp(name, bind->name))
				load(bind, value);
		}
	}
}

static void kb_load_key(Bindings* bind, yyjson_val* value) {
	bind->key = yyjson_get_int(value);
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
	if (config_file == NULL) {
		WARN("No config to load");
		return;
	}

	yyjson_read_err error;
	yyjson_doc* json = yyjson_read_file(config_file, JSON_READ_FLAGS, NULL, &error);
	if (json == NULL) {
		WTF("Failed to load config: %s", error.msg);
		return;
	}

	yyjson_val* root = yyjson_doc_get_root(json);
	if (yyjson_is_obj(root))
		parse_config(root);
	else
		WARN("Config loading skipped, has to be a key-value mapping (got %s)", yyjson_get_type_desc(root));
	yyjson_doc_free(json);
}

static yyjson_mut_val* kb_serialize_key(yyjson_mut_doc* json, const Bindings* bind) {
	return yyjson_mut_int(json, bind->key);
}

static void save_kb(yyjson_mut_doc* json, yyjson_mut_val* root, const char* name,
	yyjson_mut_val* (*serialize)(yyjson_mut_doc*, const Bindings*)) {
	yyjson_mut_val* obj = yyjson_mut_obj(json);

	for (size_t i = 0; i < KB_SIZE; i++) {
		const Bindings* binding = &BINDS[i];
		yyjson_mut_val* key = yyjson_mut_str(json, binding->name);
		yyjson_mut_obj_add(obj, key, serialize(json, binding));
	}

	yyjson_mut_val* key = yyjson_mut_str(json, name);
	yyjson_mut_obj_add(root, key, obj);
}

void save_config() {
	if (config_path != NULL) {
		WARN("Cannot save config outside of user path");
		return;
	}

	yyjson_mut_doc* json = yyjson_mut_doc_new(NULL);
	yyjson_mut_val* root = yyjson_mut_obj(json);
	yyjson_mut_doc_set_root(json, root);

	for (size_t i = 0; i < sizeof(OPTIONS) / sizeof(*OPTIONS); i++) {
		const ConfigOption* opt = &OPTIONS[i];

		yyjson_mut_val* key = yyjson_mut_strcpy(json, opt->name);
		yyjson_mut_val* value = NULL;

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

	size_t size;
	yyjson_write_err error;
	char* buffer = yyjson_mut_write_opts(json, JSON_WRITE_FLAGS, NULL, &size, &error);
	yyjson_mut_doc_free(json);

	if (buffer == NULL) {
		WTF("save_config fail: %s", error.msg);
		return;
	}
	const char* path = fmt("%sconfig.json", get_user_path());
	if (!SDL_SaveFile(path, buffer, size)) {
		WTF("save_config fail: %s", SDL_GetError());
		return;
	}
	SDL_free(buffer);
	INFO("Config saved");
}
