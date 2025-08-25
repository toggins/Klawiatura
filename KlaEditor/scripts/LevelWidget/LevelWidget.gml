function LevelWidget(_x, _y) : Widget(_x, _y) constructor {
	width = 272
	height = 8
	
	static push_field = function (_y, _name, _value, _callback = undefined) {
		add_element(new TextElement(8, _y, _name))
		var _input_x = 16 + string_width(_name)
		add_element(new InputElement(_input_x, _y, 264 - _input_x, 22, _value, _callback))
	}
	
	var _level = global.level
	var _yy = height
	
	push_field(_yy, "Name", _level.name, function (_value) {
		global.level.name = string_copy(_value, 1, 32)
		return true
	})
	_yy += 24
	
	push_field(_yy, "HUD Texture", _level.texture, function (_value) {
		global.level.texture = string_copy(_value, 1, 8)
		return true
	})
	_yy += 24
	
	push_field(_yy, "Next Level", _level.next, function (_value) {
		global.level.next = string_copy(_value, 1, 8)
		return true
	})
	_yy += 32
	
	push_field(_yy, "Width", _level.size[0], function (_value) {
		try {
			global.level.size[0] = real(_value)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Height", _level.size[1], function (_value) {
		try {
			global.level.size[1] = real(_value)
			return true
		}
		return false
	})
	_yy += 32
	
	push_field(_yy, "Left Bound", _level.bounds[0], function (_value) {
		try {
			global.level.bounds[0] = real(_value)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Top Bound", _level.bounds[1], function (_value) {
		try {
			global.level.bounds[1] = real(_value)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Right Bound", _level.bounds[2], function (_value) {
		try {
			global.level.bounds[2] = real(_value)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Bottom Bound", _level.bounds[3], function (_value) {
		try {
			global.level.bounds[3] = real(_value)
			return true
		}
		return false
	})
	_yy += 22
	
	height = _yy + 8
}