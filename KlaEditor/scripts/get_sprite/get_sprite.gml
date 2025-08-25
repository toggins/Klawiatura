function get_sprite(_name) {
	gml_pragma("forceinline")
	return global.sprites[? _name] ?? sprEmpty
}