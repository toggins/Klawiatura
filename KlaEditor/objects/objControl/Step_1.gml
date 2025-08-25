var _widget = global.widget
if _widget != undefined {
	with _widget {
		if mouse_check_button(mb_right) {
			x = clamp(window_mouse_get_x(), 0, window_get_width() - width)
			y = clamp(window_mouse_get_y(), 0, window_get_height() - height)
		}
		_widget.tick()
	}
	
	var _element_focus = global.element_focus
	if _element_focus != undefined
		_element_focus.tick()
	
	global.widget_step = true
	exit
}

if keyboard_check(vk_f1) {
	var _file = get_open_filename_ext("All files|*.*", "", blueprint_path, "Open blueprint image");
	
	if sprite_exists(blueprint) {
		sprite_delete(blueprint)
		blueprint = -1
	}
	
	if _file != "" {
		blueprint = sprite_add(_file, 1, false, false, 0, 0)
		blueprint_path = filename_path(_file)
	}
}

if keyboard_check(vk_shift) {
	cursor_x = mouse_x
	cursor_y = mouse_y
} else {
	cursor_x = floor(mouse_x / grid_size) * grid_size
	cursor_y = floor(mouse_y / grid_size) * grid_size
}

if mouse_check_button_pressed(mb_middle) {
	drag_x = window_mouse_get_x()
	drag_y = window_mouse_get_y()
}
if mouse_check_button(mb_middle) {
	var _camera = view_camera[0]
	var _dx = camera_get_view_x(_camera)
	var _dy = camera_get_view_y(_camera)
	var _x = window_mouse_get_x()
	var _y = window_mouse_get_y()
	
	_dx -= (_x - drag_x) * zoom
	_dy -= (_y - drag_y) * zoom
	drag_x = _x
	drag_y = _y
	camera_set_view_pos(_camera, _dx, _dy)
}

var _zoom = mouse_wheel_down() - mouse_wheel_up()
if _zoom != 0 {
	var _camera = view_camera[0]
	var _inc = zoom >= 1 ? _zoom * 0.2 : _zoom * 0.1
	zoom = clamp(zoom + _inc, 0.1, 10)
	camera_set_view_size(_camera, window_width * zoom, window_height * zoom)
}

if keyboard_check_pressed(ord("R")) {
	var _camera = view_camera[0]
	camera_set_view_pos(_camera, 0, 0)
	camera_set_view_size(_camera, window_width, window_height)
	zoom = 1
	grid_size = 32
}

if keyboard_check_pressed(ord("G"))
	switch grid_size {
		case 1: grid_size = 16 break
		default:
		case 16: grid_size = 32 break
		case 32: grid_size = 64 break
		case 64: grid_size = 1 break
	}

if keyboard_check_pressed(vk_space)
	global.widget = new FolderWidget(window_mouse_get_x(), window_mouse_get_y(), global.root_folder)
else if keyboard_check_pressed(ord("Q")) and global.last_folder != undefined
	global.widget = new FolderWidget(window_mouse_get_x(), window_mouse_get_y(), global.last_folder)