var _indicators
var _widget = global.widget
var _highlighted = global.highlighted
var _stretched = global.stretched

draw_set_font(fntMain)
if _widget != undefined {
	_widget.draw(0, 0)
	_indicators = _widget.indicators("")
	
	var _element_focus = global.element_focus
	if _element_focus != undefined
		_indicators = _element_focus.indicators(_indicators)
} else {
	draw_text(16, 16, $"X: {cursor_x}\nY: {cursor_y}\n{round((1 / zoom) * 100)}%")
	_indicators = "[Space] Menu"
	if global.last_folder != undefined
	_indicators += "\n[Q] Open Last Folder"
	_indicators += "\n[Shift] Unsnap Cursor (Hold)"
	_indicators += $"\n[G] Grid Size ({grid_size})"
	_indicators += "\n[R] Reset View"
	
	if instance_exists(_highlighted) and not instance_exists(_stretched) {
		_indicators += "\n[LMB] Inspect Marker"
		_indicators += "\n[RMB] Remove Marker"
		_indicators += "\n[Ctrl] Ignore Highlight (Hold)"
		_indicators += "\n[Alt] Spam Mode (Hold)"
	}
}

var _text_y = window_height - 16
var _def = global.def

draw_set_halign(fa_right)
draw_text(window_width - 16, 16, $"{instance_number(objMarker)} markers")
draw_set_valign(fa_bottom)

if _def != undefined {
	draw_sprite_stretched(_def.sprite, 0, window_width - 80, window_height - 80, 64, 64)
	if _widget == undefined and not instance_exists(_highlighted) and not instance_exists(_stretched) {
		_indicators += "\n[LMB] Place Marker"
		_indicators += "\n[Ctrl] Ignore Highlight (Hold)"
		_indicators += "\n[Alt] Spam Mode (Hold)"
	}
}

draw_set_font(fntStatus)
draw_set_halign(fa_left)
draw_text_color(16, window_height - 16, _indicators, c_white, c_white, c_white, c_white, 0.75)
draw_set_valign(fa_top)
draw_set_font(-1)