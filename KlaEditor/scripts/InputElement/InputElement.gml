function InputElement(_x, _y, _width, _height, _value, _callback) : Element(_x, _y) constructor {
	width = _width
	height = _height
	default_width = _width
	default_height = _height
	
	value = string(_value)
	callback = _callback
	
	static on_click = function () {
		if global.element_focus != self
			keyboard_string = string(value)
	}
	
	static tick = function () {
		if global.element_focus != self
			exit
		
		if keyboard_check(vk_control) {
			if keyboard_check_pressed(ord("C"))
				clipboard_set_text(keyboard_string)
			if keyboard_check_pressed(ord("V"))
				keyboard_string += clipboard_get_text()
		}
		
		if keyboard_check_pressed(vk_escape)
			global.element_focus = undefined

		if keyboard_check_pressed(vk_enter) {
			var _new = keyboard_string
			if callback(_new) {
				value = _new
				global.element_focus = undefined
			}
		}
	}
	
	static draw = function (_x, _y) {
		var _x1 = _x + x
		var _y1 = _y + y
		var _str, _color
		
		if global.element_focus == self {
			_str = keyboard_string
			if (current_time % 1000) < 500
				_str += "_"
			_color = c_yellow
		} else {
			_str = value
			_color = c_orange
		}
		
		var _w = widget.width - x
		
		width = max(string_width_ext(_str, -1, _w), default_width)
		height = max(string_height_ext(_str, -1, _w), default_height)
		draw_rectangle_color(_x1, _y1, _x1 + width, _y1 + height, c_black, c_black, c_black, c_black, false)
		draw_text_ext_color(_x1, _y1, _str, -1, _w, _color, _color, _color, _color, 1)
	}

	static indicators = function (_str) {
		return _str + "\n[Enter] Confirm Input\n[Escape] Cancel Input"
	}
}