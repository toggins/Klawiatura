function buffer_write_char(_buffer, _string, _length = string_byte_length(_string)){
	var _strlen = string_byte_length(_string)
	var i = 1
	repeat _length {
		buffer_write(_buffer, buffer_u8, i > _strlen ? 0 : string_byte_at(_string, i));
		++i
	}
}