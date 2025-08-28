function save_level(_filename) {
	if _filename == ""
		exit
	
	var _kla = buffer_create(1, buffer_grow, 1)
	
	// Header
	buffer_write_char(_kla, "Klawiatura", 10) // char[10]
	buffer_write(_kla, buffer_u8, MAJOR_FILE_VERSION) // uint8_t
	buffer_write(_kla, buffer_u8, MINOR_FILE_VERSION) // uint8_t
	
	// Level
	with global.level {
		buffer_write_char(_kla, name, 32) // char[32]
		buffer_write_char(_kla, texture, 8) // char[8]
		buffer_write_char(_kla, next, 8) // char[8]
		buffer_write_char(_kla, track[0], 8) // char[8]
		buffer_write_char(_kla, track[1], 8) // char[8]
		buffer_write(_kla, buffer_u16, flags) // uint16_t
		buffer_write(_kla, buffer_s32, round(size[0] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(size[1] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(bounds[0] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(bounds[1] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(bounds[2] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(bounds[3] * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, time) // int32_t
		buffer_write(_kla, buffer_s32, round(water * 65536)) // int32_t (Q16.16)
		buffer_write(_kla, buffer_s32, round(hazard * 65536)) // int32_t (Q16.16)
	}
	
	// Markers
	var _markers = global.markers
	var n = ds_list_size(_markers)
	buffer_write(_kla, buffer_u32, n) // uint32_t
	
	var i = 0
	repeat n
		with _markers[| i++] {
			buffer_write(_kla, buffer_string, def.name)
			if is_instanceof(def, GradientDef) {
				buffer_write(_kla, buffer_u8, DefTypes.GRADIENT)
				buffer_write_char(_kla, def.sprite_name, 8)
				buffer_write(_kla, buffer_f32, x) // float
				buffer_write(_kla, buffer_f32, y) // float
				buffer_write(_kla, buffer_f32, x + image_xscale) // float
				buffer_write(_kla, buffer_f32, y + image_yscale) // float
				buffer_write(_kla, buffer_f32, depth) // float
				
				var j = 0
				repeat 4 {
					var _color = colors[j]
					var _alpha = alphas[j]
					buffer_write(_kla, buffer_u8, color_get_red(_color)) // uint8_t
					buffer_write(_kla, buffer_u8, color_get_green(_color)) // uint8_t
					buffer_write(_kla, buffer_u8, color_get_blue(_color)) // uint8_t
					buffer_write(_kla, buffer_u8, round(_alpha * 255)); // uint8_t
					++j
				}
			} else if is_instanceof(def, BackdropDef) {
				buffer_write(_kla, buffer_u8, DefTypes.BACKDROP)
				buffer_write_char(_kla, def.sprite_name, 8)
				buffer_write(_kla, buffer_f32, x) // float
				buffer_write(_kla, buffer_f32, y) // float
				buffer_write(_kla, buffer_f32, depth) // float
				buffer_write(_kla, buffer_f32, image_xscale) // float
				buffer_write(_kla, buffer_f32, image_yscale) // float
				buffer_write(_kla, buffer_u8, color_get_red(image_blend)) // uint8_t
				buffer_write(_kla, buffer_u8, color_get_green(image_blend)) // uint8_t
				buffer_write(_kla, buffer_u8, color_get_blue(image_blend)) // uint8_t
				buffer_write(_kla, buffer_u8, round(image_alpha * 255)) // uint8_t
			} else if is_instanceof(def, ObjectDef) {
				buffer_write(_kla, buffer_u8, DefTypes.OBJECT)
				buffer_write(_kla, buffer_u16, def.index)
				buffer_write(_kla, buffer_s32, round(x * 65536)) // int32_t (Q16.16)
				buffer_write(_kla, buffer_s32, round(y * 65536)) // int32_t (Q16.16)
				buffer_write(_kla, buffer_s32, round(depth * 65536)) // int32_t (Q16.16)
				buffer_write(_kla, buffer_s32, round(image_xscale * 65536)) // int32_t (Q16.16)
				buffer_write(_kla, buffer_s32, round(image_yscale * 65536)) // int32_t (Q16.16)
				
				var _num_values = 0
				var j = 0
				repeat array_length(values) {
					if values[j] != undefined {
						++_num_values
					}
					++j
				}
				buffer_write(_kla, buffer_u8, _num_values) // uint8_t
				
				j = 0
				repeat array_length(values) {
					var _value = values[j]
					if _value != undefined {
						buffer_write(_kla, buffer_u8, j)
						buffer_write(_kla, buffer_s32, _value);
					}
					++j
				}
				
				buffer_write(_kla, buffer_u32, flags)
			}
		}
	
	buffer_save_ext(_kla, _filename, 0, buffer_tell(_kla))
	buffer_delete(_kla)
	global.last_name = filename_name(_filename)
	global.last_path = filename_path(_filename)
	show_debug_message($"Saved level \"{global.last_name}\"")
}