if sprite_exists(blueprint) {
	var _depth = gpu_get_depth()
	gpu_set_depth(1000)
	draw_sprite_ext(blueprint, 0, 0, 0, 1, 1, 0, c_white, 0.5)
	gpu_set_depth(_depth)
}