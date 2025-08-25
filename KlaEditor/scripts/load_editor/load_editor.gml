function load_editor() {
	var _editor_file = global.data_path + "editor.json"
	if not file_exists(_editor_file)
		return false
	
	var _json = load_json(_editor_file)
	if not is_struct(_json)
		return false
	
	
	
	return true
}