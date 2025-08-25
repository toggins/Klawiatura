var _camera = view_camera[0]
var _x = camera_get_view_x(_camera)
var _y = camera_get_view_y(_camera)
var _w = camera_get_view_width(_camera)
var _h = camera_get_view_height(_camera)

var _x2 = _x + _w
var _y2 = _y + _h

if grid_size > 1 {
	draw_set_color(c_black)
	draw_set_alpha(0.2)
	for (var i = floor(_x / grid_size) * grid_size; i <= _x2; i += grid_size)
		draw_line(i, _y - grid_size, i, _y2 + grid_size)
	for (var i = floor(_y / grid_size) * grid_size; i <= _y2; i += grid_size)
		draw_line(_x - grid_size, i, _x2 + grid_size, i)
	draw_set_color(c_white)
	draw_set_alpha(1)
}

with global.level {
	draw_set_alpha(0.5)
	draw_rectangle(0, 0, size[0], size[1], true)
	draw_rectangle(bounds[0], bounds[1], bounds[2], bounds[3], true)
	draw_set_alpha(1)
}

var _highlighted = global.highlighted
if instance_exists(_highlighted) {
	with _highlighted {
		draw_set_alpha(0.5)
		draw_rectangle(bbox_left, bbox_top, bbox_right, bbox_bottom, true)
		draw_set_alpha(1)
	}
} else if global.widget == undefined {
	var _def = global.def
	if _def != undefined
		draw_sprite_ext(_def.sprite, 0, cursor_x, cursor_y, 1, 1, 0, c_white, 0.5)
}