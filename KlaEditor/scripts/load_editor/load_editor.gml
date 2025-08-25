function load_editor() {
	var _json = force_type(load_json(global.data_path + "editor.json"), "struct")
	var _defs = force_type(_json[$ "defs"], "array")
	var _root_folder = global.root_folder ?? new Folder("")
	
	_root_folder.add_items(_defs)
	global.root_folder = _root_folder
}