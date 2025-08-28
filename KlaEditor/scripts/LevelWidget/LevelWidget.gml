function LevelWidget(_x, _y) : Widget(_x, _y) constructor {
	width = 272
	height = 8
	
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
	
	push_field(_yy, "Primary Track", _level.track[0], function (_value) {
		global.level.track[0] = string_copy(_value, 1, 8)
		return true
	})
	_yy += 24
	
	push_field(_yy, "Secondary Track", _level.track[1], function (_value) {
		global.level.track[1] = string_copy(_value, 1, 8)
		return true
	})
	_yy += 32
	
	push_field(_yy, "Width", _level.size[0], function (_value) {
		try {
			global.level.size[0] = clamp(real(_value), 0, 32767)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Height", _level.size[1], function (_value) {
		try {
			global.level.size[1] = clamp(real(_value), 0, 32767)
			return true
		}
		return false
	})
	_yy += 32
	
	push_field(_yy, "Left Bound", _level.bounds[0], function (_value) {
		try {
			global.level.bounds[0] = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Top Bound", _level.bounds[1], function (_value) {
		try {
			global.level.bounds[1] = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Right Bound", _level.bounds[2], function (_value) {
		try {
			global.level.bounds[2] = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Bottom Bound", _level.bounds[3], function (_value) {
		try {
			global.level.bounds[3] = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 32
	
	push_field(_yy, "Time", _level.time, function (_value) {
		try {
			global.level.time = clamp(round(real(_value)), -1, 2147483647)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Water Y", _level.water, function (_value) {
		try {
			global.level.water = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 24
	
	push_field(_yy, "Hazard Y", _level.hazard, function (_value) {
		try {
			global.level.hazard = clamp(real(_value), -32768, 32767)
			return true
		}
		return false
	})
	_yy += 32
	
	push_button(_yy, "8-3 Lava?", _level.flags & LevelFlags.LAVA, function () {
		with global.level {
			if (flags & LevelFlags.LAVA)
				flags &= ~LevelFlags.LAVA
			else
				flags |= LevelFlags.LAVA
			return (flags & LevelFlags.LAVA)
		}
		return false
	})
	_yy += 24
	
	push_button(_yy, "Hardcore?", _level.flags & LevelFlags.HARDCORE, function () {
		with global.level {
			if (flags & LevelFlags.HARDCORE)
				flags &= ~LevelFlags.HARDCORE
			else
				flags |= LevelFlags.HARDCORE
			return (flags & LevelFlags.HARDCORE)
		}
		return false
	})
	_yy += 24
	
	push_button(_yy, "Spike Ceiling?", _level.flags & LevelFlags.SPIKES, function () {
		with global.level {
			if (flags & LevelFlags.SPIKES)
				flags &= ~LevelFlags.SPIKES
			else
				flags |= LevelFlags.SPIKES
			return (flags & LevelFlags.SPIKES)
		}
		return false
	})
	_yy += 24
	
	push_button(_yy, "Lost Map?", _level.flags & LevelFlags.LOST, function () {
		with global.level {
			if (flags & LevelFlags.LOST)
				flags &= ~LevelFlags.LOST
			else
				flags |= LevelFlags.LOST
			return (flags & LevelFlags.LOST)
		}
		return false
	})
	_yy += 24
	
	push_button(_yy, "Funny Tanks?", _level.flags & LevelFlags.FUNNY, function () {
		with global.level {
			if (flags & LevelFlags.FUNNY)
				flags &= ~LevelFlags.FUNNY
			else
				flags |= LevelFlags.FUNNY
			return (flags & LevelFlags.FUNNY)
		}
		return false
	})
	_yy += 22
	
	height = _yy + 8
}