function Element(_x, _y) constructor {
	x = _x
	y = _y
	
	width = 16
	height = 16
	
	window = undefined
	
	static on_click = function () {}
	
	static tick = function () {}
	
	static draw = function (_x, _y) {}
	
	static indicators = function (_str) {
		return _str
	}
	
	static set_focus = function (_click) {
		if _click
			on_click()
		global.override_element_focus = true
		global.element_focus = self
	}
}