var _config = load_json(game_save_id + "config.json")
var _data_path = is_struct(_config) ? _config.data_path : program_directory

var _save = false
while not file_exists(_data_path + "editor.json") {
	show_message("Invalid resource path, please set up the path to Klawiatura's editor.json")
	
	var _file = get_open_filename_ext("Editor definitions|editor.json", "", _data_path, "Path to editor.json")
	if _file == "" {
		game_end(1)
		exit
	}
	_data_path = filename_path(_file)
	_save = true
}
if _save {
	var _buffer = buffer_create(1, buffer_grow, 1)
	buffer_write(_buffer, buffer_text, json_stringify({data_path: _data_path}, true))
	buffer_save_ext(_buffer, game_save_id + "config.json", 0, buffer_tell(_buffer))
	buffer_delete(_buffer)
}

global.data_path = _data_path
load_editor()

global.last_name = "UNTITLED.kla"
global.last_path = _data_path

global.widget = undefined
global.widget_step = false
global.element_focus = undefined
global.override_element_focus = false

global.def = undefined

update_highlight = false
highlight_list = ds_list_create()
global.highlighted = noone
global.stretched = noone

blueprint = -1
blueprint_path = _data_path

window_width = window_get_width()
window_height = window_get_height()

drag_x = 0
drag_y = 0
zoom = 1

cursor_x = 0
cursor_y = 0
highlight_x = 0
highlight_y = 0
grid_size = 32

application_surface_enable(false)
gpu_set_tex_repeat(true)