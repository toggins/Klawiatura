if is_instanceof(def, GradientDef) {
	draw_primitive_begin_texture(pr_trianglestrip, texture)
	var _x1 = bbox_left
	var _y1 = bbox_top
	var _x2 = bbox_right
	var _y2 = bbox_bottom
	var _u = (_x2 - _x1) / texture_get_width(texture)
	var _v = (_y2 - _y1) / texture_get_height(texture)
	
	draw_vertex_texture_colour(_x1, _y2, 0, _v, colors[2], alphas[2])
	draw_vertex_texture_colour(_x2, _y2, _u, _v, colors[3], alphas[3])
	draw_vertex_texture_colour(_x1, _y1, 0, 0, colors[0], alphas[0])
	draw_vertex_texture_colour(_x2, _y1, _u, 0, colors[1], alphas[1])
	draw_primitive_end()
} else {
	draw_self()
}