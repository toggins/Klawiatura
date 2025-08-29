function PropertiesWidget(_x, _y, _title, _marker = noone) : Widget(_x, _y) constructor {
	width = max(272, 16 + string_width(_title))
	height = 16 + string_height(_title)
	
	title = _title
	marker = _marker
	
	add_element(new TextElement(8, 8, _title))
	
	static push_field = function (_y, _name, _value, _callback) {
		add_element(new TextElement(8, _y, _name))
		var _input_x = 16 + string_width(_name)
		add_element(new InputElement(_input_x, _y, 264 - _input_x, 22, _value, _callback))
	}
	
	static push_button = function (_y, _name, _value, _callback) {
		add_element(new TextElement(8, _y, _name))
		
		var _button = new ButtonElement(16 + string_width(_name), _y, _value ? "ON" : "OFF", undefined)
		_button.on_click = method({
			me: _button,
			callback: _callback,
		}, function() {
			me.value = callback() ? "ON" : "OFF"
		})
		add_element(_button)
	}
	
	if not instance_exists(_marker)
		exit
	
	var _def = _marker.def
	if is_instanceof(_def, GradientDef) {
		var _yy = height
		
		push_field(_yy, "Width", _marker.image_xscale, method(_marker, function (_value) {
			try {
				image_xscale = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Height", _marker.image_yscale, method(_marker, function (_value) {
			try {
				image_yscale = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 32
		
		push_field(_yy, "Top-left Color", _marker.colors[0], method(_marker, function (_value) {
			try {
				colors[0] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Top-left Alpha", _marker.alphas[0], method(_marker, function (_value) {
			try {
				alphas[0] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		push_field(_yy, "Top-right Color", _marker.colors[1], method(_marker, function (_value) {
			try {
				colors[1] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Top-right Alpha", _marker.alphas[1], method(_marker, function (_value) {
			try {
				alphas[1] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		push_field(_yy, "Bottom-left Color", _marker.colors[2], method(_marker, function (_value) {
			try {
				colors[2] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Bottom-left Alpha", _marker.alphas[2], method(_marker, function (_value) {
			try {
				alphas[2] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		push_field(_yy, "Bottom-right Color", _marker.colors[3], method(_marker, function (_value) {
			try {
				colors[3] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Bottom-right Alpha", _marker.alphas[3], method(_marker, function (_value) {
			try {
				alphas[3] = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 22
		
		height = _yy + 8
	} else if is_instanceof(_def, BackdropDef) {
		var _yy = height
		
		push_field(_yy, "X Scale", _marker.image_xscale, method(_marker, function (_value) {
			try {
				image_xscale = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Y Scale", _marker.image_yscale, method(_marker, function (_value) {
			try {
				image_yscale = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 32
		
		push_field(_yy, "Color", _marker.image_blend, method(_marker, function (_value) {
			try {
				image_blend = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Alpha", _marker.image_alpha, method(_marker, function (_value) {
			try {
				image_alpha = real(_value)
				return true
			} catch (e) {}
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
			} catch (e) {}
			return false
		}))
		_yy += 24
		
		push_field(_yy, "Y Scale", _marker.image_yscale, method(_marker, function (_value) {
			try {
				image_yscale = real(_value)
				return true
			} catch (e) {}
			return false
		}))
		_yy += 32
		
		var _values = _def.values
		var i = 0
		repeat array_length(_values) {
			var _value = _values[i]
			push_field(_yy, _value.name, _marker.values[_value.index] ?? "", method({
				marker: _marker,
				index: _value.index
			}, function (_value) {
				try {
					marker.values[index] = real(_value)
					return true
				} catch (e) {}
				return false
			}))
			_yy += 24;
			++i
		}
		
		var _flags = _def.flags
		i = 0
		repeat array_length(_flags) {
			var _flag = _flags[i]
			push_button(_yy, _flag.name, _marker.flags & _flag.bit, method({
				marker: _marker,
				bit: _flag.bit
			}, function () {
				if (marker.flags & bit)
					marker.flags &= ~bit
				else
					marker.flags |= bit
				return (marker.flags & bit)
			}))
			_yy += 24;
			++i
		}
		
		height = _yy + 6
	}
}