function force_type(_value, _type) {
	gml_pragma("forceinline")
	
	var _typeof = typeof(_value)
	if _typeof != _type
		show_error($"!!! force_type: Expected {_type}, got {_typeof}", true)
	
	return _value
}