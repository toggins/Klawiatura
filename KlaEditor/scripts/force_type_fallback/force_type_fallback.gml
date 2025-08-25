function force_type_fallback(_value, _type, _fallback = undefined) {
	gml_pragma("forceinline")
	return typeof(_value) == _type ? _value : _fallback
}