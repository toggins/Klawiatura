function SpriteElement(_x, _y, _sprite = -1, _width = undefined, _height = undefined) : Element(_x, _y) constructor {
	sprite = _sprite
	width = _width ?? sprite_get_width(sprite)
	height = _height ?? sprite_get_height(sprite)
	
	static draw = function (_x, _y) {
		if sprite_exists(sprite)
			draw_sprite_stretched(sprite, 0, _x + x, _y + y, width, height)
	}
}