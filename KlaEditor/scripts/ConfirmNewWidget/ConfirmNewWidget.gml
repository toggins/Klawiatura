function ConfirmNewWidget(_x, _y) : Widget(_x, _y) constructor {
	width = 64
	height = 30
	
	add_element(new ButtonElement(4, 4, "Do it!", function () {
		clear_level()
		close()
	}))
}