#include "K_config.h"
#include "K_audio.h"
#include "K_file.h"
#include "K_input.h"
#include "K_log.h"
#include "K_video.h"

typedef struct {
	const char* name;
	void (*h_bool)(bool), (*h_int)(int), (*h_float)(float);
} ConfigOption;

static int cfg_width = 0, cfg_height = 0;
static void set_width(int width) {
	cfg_width = width;
	if (cfg_height)
		set_resolution(cfg_width, cfg_height);
}
static void set_height(int height) {
	cfg_height = height;
	if (cfg_width)
		set_resolution(cfg_width, cfg_height);
}

static const ConfigOption OPTIONS[] = {
	{"width",        .h_int = set_width         },
	{"height",       .h_int = set_height        },
	{"fullscreen",   .h_bool = set_fullscreen   },
	{"vsync",        .h_bool = set_vsync        },
	{"volume",       .h_float = set_volume      },
	{"sound_volume", .h_float = set_sound_volume},
	{"music_volume", .h_float = set_music_volume},
};

static const char* config_path = NULL;

void config_init(const char* path) {
	config_path = path;
	if (config_path != NULL)
		INFO("Using config \"%s\"", config_path);
	load_config();
}

void config_teardown() {}

#define HANDLE_OPT(fn, conversion)                                                                                     \
	if (opt->fn != NULL) {                                                                                         \
		opt->fn(conversion(value));                                                                            \
		goto next_opt;                                                                                         \
	}
#define HANDLE_KB(prefix, field, conversion)                                                                           \
	if (SDL_strlen(name) > SDL_strlen(prefix) + 1 && !SDL_memcmp(name, prefix "_", SDL_strlen(prefix) + 1)         \
		&& !SDL_strcmp(name + SDL_strlen(prefix) + 1, BINDS[i].name))                                          \
	{                                                                                                              \
		BINDS[i].field = (conversion(value));                                                                  \
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
			HANDLE_OPT(h_bool, yyjson_get_bool);
			HANDLE_OPT(h_int, yyjson_get_int);
			HANDLE_OPT(h_float, yyjson_get_real);
		}
		for (int i = 0; i < KB_SIZE; i++) {
			if (BINDS[i].name == NULL)
				continue;
			HANDLE_KB("kbd", key, yyjson_get_int);
		}
	next_opt:
		continue;
	}
}
#undef HANDLE_KB
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
