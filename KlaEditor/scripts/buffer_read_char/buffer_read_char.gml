function buffer_read_char(_buffer, _length){
	var _string = ""
	var i = 1
	repeat _length {
		var _byte = buffer_read(_buffer, buffer_u8)
		if _byte > 0
			_string = string_set_byte_at(_string + " ", i, _byte);
		++i
	}
	
	return _string
}