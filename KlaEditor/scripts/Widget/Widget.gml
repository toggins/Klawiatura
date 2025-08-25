function Widget(_x, _y) constructor {
	x = _x
	y = _y
	
	width = 128
	height = 128
	
	child = undefined
	parent = undefined
	elements = []
	popup = true
	
	clicked = false
	
	static handle_close = function () {
		return true
	}
	
	static add_element = function (_element) {
		_element.widget = self
		array_push(elements, _element)
	}
	
	static link_widget = function (_widget) {
		if child != undefined and not child.close()
			return false
		
		_widget.parent = self
		child = _widget
		return true
	}
	
	static close = function () {
		if child != undefined {
			if child.close()
				child = undefined
			else
				return false
		}
		
		if not handle_close()
			return false
		if parent != undefined and parent.child == self
			parent.child = undefined
		
		if global.widget == self
			global.widget = undefined
		
		var _element_focus = global.element_focus
		if _element_focus != undefined and _element_focus.widget == self {
			global.element_focus = undefined
		}
		
		return true
	}
	
	static get_clicked = function () {
		return clicked or (child != undefined and child.get_clicked())
	}
	
	static tick = function (_x = 0, _y = 0) {
		_x += x
		_y += y
		
		if child != undefined
			child.tick(_x, _y)
		
		if mouse_check_button_pressed(mb_left) {
			var _mx = window_mouse_get_x()
			var _my = window_mouse_get_y()
			
			if point_in_rectangle(_mx, _my, _x, _y, _x + width, _y + height) {
				clicked = true
			} else {
				if popup and not get_clicked() {
					close()
					exit
				}
				clicked = false
			}
			
			var i = 0
			repeat array_length(elements) {
				with elements[i++] {
					var _x1 = x + _x
					var _y1 = y + _y
					
					if point_in_rectangle(_mx, _my, _x1, _y1, _x1 + width, _y1 + height) {
						on_click()
						if global.override_element_focus
							global.override_element_focus = false
						else
							global.element_focus = self
						other.clicked = true
					}
				}
			}
		} else {
			clicked = false
		}
	}
	
	static draw = function (_x, _y) {
		_x += x
		_y += y
		draw_rectangle_color(_x, _y, _x + width, _y + height, c_dkgray, c_dkgray, c_dkgray, c_dkgray, false)
		
		var i = 0
		repeat array_length(elements)
			elements[i++].draw(_x, _y)
		
		if child != undefined
			child.draw(_x, _y)
	}

	static indicators = function (_str) {
		return _str + "\n[RMB] Move Widget (Hold)"
	}
}