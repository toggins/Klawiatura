function ButtonElement(_x, _y, _text, _callback) : TextElement(_x, _y, _text) constructor {
	on_click = _callback ?? on_click
	
	static TextElement_draw = static_get(static_get(self)).draw
	
	static draw = function (_x, _y) {
		TextElement_draw(_x, _y)
		
		var _x1 = _x + x
		var _y1 = _y + y
		var _x2 = _x1 + width
		var _y2 = _y1 + height
		
		if point_in_rectangle(window_mouse_get_x(), window_mouse_get_y(), _x1, _y1, _x2, _y2) {
			draw_set_alpha(0.5)
			draw_rectangle(_x1, _y1, _x2, _y2, false)
			draw_set_alpha(1)
		}
	}
}