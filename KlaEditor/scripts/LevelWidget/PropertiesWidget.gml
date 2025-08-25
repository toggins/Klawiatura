function PropertiesWidget(_x, _y, _title, _marker = noone) : Widget(_x, _y) constructor {
	width = max(272, 16 + string_width(_title))
	height = 16 + string_height(_title)
	
	title = _title
	marker = _marker
	
	add_element(new TextElement(8, 8, _title))
	
	static push_field = function (_y, _name, _value, _callback = undefined) {
		add_element(new TextElement(8, _y, _name))
		var _input_x = 16 + string_width(_name)
		add_element(new InputElement(_input_x, _y, 264 - _input_x, 22, _value, _callback))
	}
	
	if not instance_exists(_marker)
		exit
	
	var _def = _marker.def
	if is_instanceof(_def, BackdropDef) {
		var _yy = height
		
		push_field(_yy, "Color", _marker.image_blend, method(_marker, function (_value) {
			try {
				image_blend = real(_value)
				return true
			}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Alpha", _marker.image_alpha, method(_marker, function (_value) {
			try {
				image_alpha = real(_value)
				return true
			}
			return false
		}))
		_yy += 22
		
		height = _yy + 8
	} else if is_instanceof(_def, ObjectDef) {
		var _yy = height
		
		push_field(_yy, "X Scale", _marker.image_xscale, method(_marker, function (_value) {
			try {
				image_xscale = real(_value)
				return true
			}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Y Scale", _marker.image_yscale, method(_marker, function (_value) {
			try {
				image_yscale = real(_value)
				return true
			}
			return false
		}))
		_yy += 22
		
		height = _yy + 8
	}
}