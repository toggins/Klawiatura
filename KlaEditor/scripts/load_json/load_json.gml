function load_json(_filename) {
	if not file_exists(_filename)
		return undefined
	
	var _buffer = buffer_load(_filename)
	var _text = buffer_read(_buffer, buffer_text)
	buffer_delete(_buffer)
	
	return json_parse(_text)
}