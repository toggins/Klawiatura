function load_level(_filename) {
	if _filename == ""
		exit
	
	var _kla = buffer_load(_filename)
	if not buffer_exists(_kla) {
		show_message($"Invalid path \"{_filename}\"!")
		exit
	}
	
	clear_level()
	
	// Header
	if buffer_read_char(_kla, 10) != "Klawiatura" {
		show_message($"Invalid header!")
		buffer_delete(_kla)
		exit
	}
	if buffer_read(_kla, buffer_u8) != MAJOR_FILE_VERSION {
		show_message($"Invalid major version!")
		buffer_delete(_kla)
		exit
	}
	if buffer_read(_kla, buffer_u8) != MINOR_FILE_VERSION {
		show_message($"Invalid minor version!")
		buffer_delete(_kla)
		exit
	}
	
	// Level
	with global.level {
		name = buffer_read_char(_kla, 32)
		texture = buffer_read_char(_kla, 8)
		next = buffer_read_char(_kla, 8)
		track[0] = buffer_read_char(_kla, 8)
		track[1] = buffer_read_char(_kla, 8)
		flags = buffer_read(_kla, buffer_u16)
		size[0] = buffer_read(_kla, buffer_s32) / 65536
		size[1] = buffer_read(_kla, buffer_s32) / 65536
		bounds[0] = buffer_read(_kla, buffer_s32) / 65536
		bounds[1] = buffer_read(_kla, buffer_s32) / 65536
		bounds[2] = buffer_read(_kla, buffer_s32) / 65536
		bounds[3] = buffer_read(_kla, buffer_s32) / 65536
		time = buffer_read(_kla, buffer_s32)
		water = buffer_read(_kla, buffer_s32) / 65536
		hazard = buffer_read(_kla, buffer_s32) / 65536
	}
	
	// Markers
	repeat buffer_read(_kla, buffer_u32) {
		var _marker = buffer_read(_kla, buffer_string)
		var _type = buffer_read(_kla, buffer_u8)
		
		switch _type {
			case DefTypes.BACKDROP: {
				buffer_read_char(_kla, 8)
				var _x = buffer_read(_kla, buffer_s32) / 65536
				var _y = buffer_read(_kla, buffer_s32) / 65536
				var _z = buffer_read(_kla, buffer_s32) / 65536
				var _x_scale = buffer_read(_kla, buffer_s32) / 65536
				var _y_scale = buffer_read(_kla, buffer_s32) / 65536
				var _r = buffer_read(_kla, buffer_u8)
				var _g = buffer_read(_kla, buffer_u8)
				var _b = buffer_read(_kla, buffer_u8)
				var _a = buffer_read(_kla, buffer_u8)
				
				with instance_create_depth(_x, _y, _z, objMarker) {
					def = global.defs[? _marker]
					if def != undefined
						sprite_index = def.sprite
					
					image_xscale = _x_scale
					image_yscale = _y_scale
					image_blend = make_color_rgb(_r, _g, _b)
					image_alpha = _a / 255
				}
				break
			}
			
			case DefTypes.OBJECT: {
				buffer_read(_kla, buffer_u16)
				var _x = buffer_read(_kla, buffer_s32) / 65536
				var _y = buffer_read(_kla, buffer_s32) / 65536
				var _z = buffer_read(_kla, buffer_s32) / 65536
				var _x_scale = buffer_read(_kla, buffer_s32) / 65536
				var _y_scale = buffer_read(_kla, buffer_s32) / 65536
				
				// TODO: Read values
				repeat buffer_read(_kla, buffer_u8) {
					buffer_read(_kla, buffer_u8)
					buffer_read(_kla, buffer_s32)
				}
				
				var _flags = buffer_read(_kla, buffer_u32)
				
				with instance_create_depth(_x, _y, _z, objMarker) {
					def = global.defs[? _marker]
					if def != undefined
						sprite_index = def.sprite
					
					image_xscale = _x_scale
					image_yscale = _y_scale
					flags = _flags
				}
				break
			}
			
			default: {
				show_message($"Unknown marker type {_type}!")
				buffer_delete(_kla)
				exit
			}
		}
	}
	
	buffer_delete(_kla)
	global.last_name = filename_name(_filename)
	global.last_path = filename_path(_filename)
	show_debug_message($"Loaded level \"{global.last_name}\"")
}