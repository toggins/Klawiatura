function fetch_sprite(_name) {
	gml_pragma("forceinline")
	load_sprite(_name)
	return get_sprite(_name)
}