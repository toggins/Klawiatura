function TextElement(_x, _y, _value) : Element(_x, _y) constructor {
	value = _value
	
	static draw = function (_x, _y) {
		var _str = string(value)
		var _w = widget.width - x
		
		width = string_width_ext(_str, -1, _w)
		height = string_height_ext(_str, -1, _w)
		draw_text_ext(_x + x, _y + y, _str, -1, _w)
	}
}