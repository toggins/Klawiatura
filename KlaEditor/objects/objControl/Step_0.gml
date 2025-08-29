if global.widget != undefined
	exit
if global.widget_step {
	global.widget_step = false
	exit
}

var _stretched = global.stretched
if keyboard_check(vk_control) or instance_exists(_stretched) {
	global.highlighted = noone
} else if update_highlight or mouse_x != highlight_x or mouse_y != highlight_y {
	highlight_x = mouse_x
	highlight_y = mouse_y
	
	var _highlighted = noone
	var n = instance_position_list(highlight_x, highlight_y, objMarker, highlight_list, false)
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
	if instance_exists(_stretched) {
		var _sprite = _stretched.sprite_index
		var _max_stretch = _stretched.def.max_stretch
		_stretched.image_xscale = min(max(16, cursor_x - _stretched.x), _max_stretch[0]) / sprite_get_width(_sprite)
		_stretched.image_yscale = min(max(16, cursor_y - _stretched.y), _max_stretch[1]) / sprite_get_height(_sprite)
		if not mouse_check_button(mb_left)
			global.stretched = noone
	} else {
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
					var _marker = instance_create_depth(cursor_x, cursor_y, _def.z, objMarker)
					with _marker {
						def = _def
						if is_instanceof(_def, GradientDef) {
							sprite_index = sprGradient
							texture = sprite_get_texture(_def.sprite, 0)
							array_copy(colors, 0, _def.colors, 0, 4)
							array_copy(alphas, 0, _def.alphas, 0, 4)
						} else {
							sprite_index = _def.sprite
							
							if is_instanceof(_def, ObjectDef) {
								var _values = _def.values
								var i = 0
								repeat array_length(_values) {
									var _value = _def.values[i]
									values[_value.index] = _value.default_value;
									++i
								}
								
								var _flags = _def.flags
								i = 0
								repeat array_length(_flags) {
									var _flag = _def.flags[i]
									flags |= (_flag.default_value * _flag.bit);
									++i
								}
							}
						}
					}
					if _def.stretch {
						global.highlighted = noone
						global.stretched = _marker
					} else {
						update_highlight = true
					}
				}
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