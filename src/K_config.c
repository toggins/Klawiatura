#include "K_config.h"
#include "K_file.h"
#include "K_log.h"
#include "K_video.h"

static const char* config_path = NULL;

void config_init(const char* path) {
	config_path = path;
	if (config_path != NULL)
		INFO("Using config \"%s\"", config_path);

	load_config();
}

void config_teardown() {}

void load_config() {
	const char* config_file = config_path == NULL ? find_user_file("config.json", NULL) : config_path;
	if (config_file == NULL) {
		WARN("No config to load");
		return;
	}

	yyjson_read_err error;
	yyjson_doc* json = yyjson_read_file(
		config_file, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, &error);
	if (json == NULL) {
		WTF("Failed to load config: %s", error.msg);
		return;
	}

	yyjson_val* root = yyjson_doc_get_root(json);
	if (yyjson_is_obj(root)) {
		set_resolution(
			yyjson_get_int(yyjson_obj_get(root, "width")), yyjson_get_int(yyjson_obj_get(root, "height")));
		set_fullscreen(yyjson_get_bool(yyjson_obj_get(root, "fullscreen")));
		set_vsync(yyjson_get_bool(yyjson_obj_get(root, "vsync")));
	}
	yyjson_doc_free(json);
}
