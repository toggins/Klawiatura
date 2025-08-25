function load_sprite(_name) {
	if ds_map_exists(global.sprites, _name)
		exit
	
	var _path = global.data_path + "textures/" + _name
	
	var _x_offset = 0
	var _y_offset = 0
	
	var _json = load_json(_path + ".json")
	if _json != undefined {
		_x_offset = force_type_fallback(_json[$ "x_offset"], "number", 0)
		_y_offset = force_type_fallback(_json[$ "y_offset"], "number", 0)
	}
	
	var _image = find_file(_path + ".*", ".json")
	if _image != ""
		global.sprites[? _name] = sprite_add(_image, 1, false, false, _x_offset, _y_offset)
}