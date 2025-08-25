draw_text(8, 8, $"X: {cursor_x}\nY: {cursor_y}\nZoom: {round((1 / zoom) * 100)}%\nMarkers: {instance_number(objMarker)}")

var _widget = global.widget
if _widget != undefined
	_widget.draw(0, 0)