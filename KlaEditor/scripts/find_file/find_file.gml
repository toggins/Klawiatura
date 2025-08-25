function find_file(_filename, _exclude = "") {
	var _target = file_find_first(_filename, fa_none)
	while _target != "" {
		if filename_ext(_target) == _exclude {
			_target = file_find_next()
			continue
		}
		
		file_find_close()
		return filename_path(_filename) + _target
	}
	
	file_find_close()
	return ""
}