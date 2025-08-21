var _ww = window_get_width()
var _wh = window_get_height()
if window_width != _ww or window_height != _wh {
	camera_set_view_size(view_camera[0], _ww * zoom, _wh * zoom)
	display_set_gui_size(_ww, _wh)
	window_width = _ww
	window_height = _wh
}