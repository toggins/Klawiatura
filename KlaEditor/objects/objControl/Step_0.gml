if global.widget != undefined
	exit
if global.widget_step {
	global.widget_step = false
	exit
}

if keyboard_check(vk_control) {
	global.highlighted = noone
} else if update_highlight or mouse_x != cursor_x or mouse_y != cursor_y {
	var _highlighted = noone
	var n = instance_position_list(cursor_x, cursor_y, objMarker, highlight_list, false)
	if n {
		var _def = global.def
		var _def_type = _def == undefined ? undefined : instanceof(_def)
		var i = 0
		repeat n {
			var _marker = highlight_list[| i++]
			if (not instance_exists(_highlighted) or _marker.depth < _highlighted.depth) and (_def == undefined or instanceof(_marker.def) == _def_type)
				_highlighted = _marker
		}
	}
	
	global.highlighted = _highlighted
	ds_list_clear(highlight_list)
	update_highlight = false
}

var _highlighted = global.highlighted
if not instance_exists(_highlighted) {
	var _spam = keyboard_check(vk_alt)
	if mouse_check_button_pressed(mb_left) or (_spam and mouse_check_button(mb_left)) {
		// Don't place if a window was closed on the last frame.
		var _def = global.def
		if _def != undefined {
			var _place = true
			if _spam and not keyboard_check(vk_shift) {
				var n = collision_rectangle_list(cursor_x, cursor_y, cursor_x + grid_size, cursor_y + grid_size, objMarker, false, true, highlight_list, false)
				if n {
					var i = 0
					repeat n {
						var _marker = highlight_list[| i++]
						if _marker.def == _def {
							_place = false
							break
						}
					}
				}
			}
			
			if _place {
				with instance_create_depth(cursor_x, cursor_y, _def.z, objMarker) {
					def = _def
					sprite_index = _def.sprite
				}
				update_highlight = true
			}
		}
	}
} else if mouse_check_button_pressed(mb_left) {
	global.widget = new PropertiesWidget(window_mouse_get_x(), window_mouse_get_y(), _highlighted.def == undefined ? "Marker" : _highlighted.def.name, _highlighted)
} else if mouse_check_button_released(mb_middle) {
	global.def = _highlighted.def
} else if mouse_check_button_pressed(mb_right) or (keyboard_check(vk_alt) and mouse_check_button(mb_right)) {
	instance_destroy(_highlighted)
	update_highlight = true
}