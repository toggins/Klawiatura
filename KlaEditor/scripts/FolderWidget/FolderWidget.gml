function FolderWidget(_x, _y, _folder) : Widget(_x, _y) constructor {
	var _path = _folder.path
	
	width = 16
	height = 16
	
	folder = _folder
	
	var _widget = self
	var _items = _folder.items
	var i = 0
	var n = array_length(_items)
	var _yy = 8
	
	repeat n {
		var _item = _items[i++]
		var _prefix = ""
		var _sprite = undefined
		
		if is_instanceof(_item, Folder) {
			_prefix = ">"
		} else if is_instanceof(_item, Def) {
			_sprite = _item.sprite
			_prefix = "  "
		}
		
		var _name = _prefix + _item.name
		add_element(new ButtonElement(8, _yy, _name, method({
			y: _yy,
			item: _item,
			widget: _widget,
		}, function () {
			if is_instanceof(item, Folder) {
				widget.link_widget(new FolderWidget(widget.width + 2, y, item))
			} else {
				global.def = item
				// global.grid_size = content.grid_size
				widget.close()
			}
		})))
		
		if _sprite != undefined
			add_element(new SpriteElement(8, _yy, _sprite, 16, 16))
		
		_yy += string_height(_name)
		width = max(width, string_width(_name) + 16)
		height = max(height, _yy + 8)
	}
	
	if _folder == global.root_folder {
		height += 108
		_yy += 11
		
		add_element(new ButtonElement(8, _yy, "Level", method({
			y: _yy,
			widget: _widget,
		}, function () {
			//widget.link_widget(new ConfirmNewWidget(widget.width + 2, y))
		})))
		
		_yy += 33
		
		add_element(new ButtonElement(8, _yy, "New", method({
			y: _yy,
			widget: _widget,
		}, function () {
			//widget.link_widget(new ConfirmNewWidget(widget.width + 2, y))
		})))
		
		add_element(new ButtonElement(8, _yy + 22, "Open", function () {
			//load_level(get_open_filename_ext("Klawiatura Level File|*.kla", global.last_name, global.data_path, "Open..."))
		}))
		
		add_element(new ButtonElement(8, _yy + 44, "Save", function () {
			//save_level(get_save_filename_ext("Klawiatura Level File|*.kla", global.last_name, global.data_path, "Save As..."))
		}))
	}
}