var _depth = gpu_get_depth()

if sprite_exists(blueprint) {
	gpu_set_depth(1000)
	draw_sprite_ext(blueprint, 0, 0, 0, 1, 1, 0, c_white, 0.5)
}

gpu_set_depth(-1000)
var _camera = view_camera[0]
var _x = camera_get_view_x(_camera)
var _y = camera_get_view_y(_camera)
var _w = camera_get_view_width(_camera)
var _h = camera_get_view_height(_camera)

var _x2 = _x + _w
var _y2 = _y + _h

if grid_size > 1 {
	draw_set_color(c_black)
	draw_set_alpha(0.25)
	for (var i = floor(_x / grid_size) * grid_size; i <= _x2; i += grid_size)
		draw_line(i, _y - grid_size, i, _y2 + grid_size)
	for (var i = floor(_y / grid_size) * grid_size; i <= _y2; i += grid_size)
		draw_line(_x - grid_size, i, _x2 + grid_size, i)
}

draw_set_color(c_yellow)
draw_set_alpha(0.5)
var _radius = 2 * zoom
draw_rectangle(cursor_x - _radius, cursor_y - _radius, cursor_x + _radius, cursor_y + _radius, false)
draw_set_color(c_white)
draw_set_alpha(1)

gpu_set_depth(_depth)