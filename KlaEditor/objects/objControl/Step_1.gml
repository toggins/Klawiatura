if keyboard_check(vk_f1) {
	var _file = get_open_filename_ext("All files|*.*", "", blueprint_path, "Open blueprint image");
	
	if sprite_exists(blueprint) {
		sprite_delete(blueprint)
		blueprint = -1
	}
	
	if _file != "" {
		blueprint = sprite_add(_file, 1, false, false, 0, 0)
		blueprint_path = filename_path(_file)
	}
}