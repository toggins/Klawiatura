function Folder(_name, _items = undefined, _from = undefined) constructor {
	name = _name
	items = []
	from = _from
	
	static get_path = function (_string = "") {
		var _path = name + "/" + _string
		return from == undefined ? _path : from.get_path(_path)
	}
	
	path = get_path()
	
	static add_items = function (_items) {
		var _defs = global.defs
		var _sprites = global.sprites
		var i = 0
		
		repeat array_length(_items) {
			var _item = _items[i++]
			
			if is_struct(_item) {
				var _type = force_type_fallback(_item[$ "type"], "string")
				switch _type {
					default: _type = DefTypes.NONE break
					case "gradient":
					case DefTypes.GRADIENT: _type = DefTypes.GRADIENT break
					case "backdrop":
					case DefTypes.BACKDROP: _type = DefTypes.BACKDROP break
					case "object":
					case DefTypes.OBJECT: _type = DefTypes.OBJECT break
				}
				
				if _type == DefTypes.NONE
					continue
				
				var _def = undefined
				switch _type {
					case DefTypes.GRADIENT: {
						_def = new GradientDef()
						_def.sprite_name = force_type_fallback(_item[$ "texture"], "string", "")
						_def.sprite = fetch_sprite(_def.sprite_name)
						break
					}
					
					case DefTypes.BACKDROP: {
						_def = new BackdropDef()
						_def.sprite_name = force_type_fallback(_item[$ "texture"], "string", "")
						_def.sprite = fetch_sprite(_def.sprite_name)
						break
					}
					
					case DefTypes.OBJECT: {
						_def = new ObjectDef()
						with _def {
							index = force_type_fallback(_item[$ "index"], "number", 0)
							sprite = fetch_sprite(force_type_fallback(_item[$ "texture"], "string", ""))
						}
						break
					}
				}
				
				var _replace = -1
				with _def {
					name = string(_item[$ "def"])
					//grid_size = real(_item[$ "grid_size"] ?? grid_size)
					z = real(_item[$ "z"] ?? z)
					_defs[? name] = _def
					
					var __items = other.items
					var j = 0
					repeat array_length(__items) {
						var _element = __items[j]
						if is_instanceof(_element, Def) and _element.name == name {
							_replace = j
							break
						}
						++j
					}
				}
				
				if _replace == -1
					array_push(items, _def)
				else
					items[_replace] = _def
			}
		
			if is_array(_item) {
				if array_length(_item) < 2
					continue
				
				var _folder_name = force_type(_item[0], "string")
				var _folder_items = force_type(_item[1], "array")
				
				var _inject = -1
				var j = 0
				repeat array_length(items) {
					var _element = items[j]
					if is_instanceof(_element, Folder) and _element.name == _folder_name {
						_inject = j
						break
					}
					++j
				}
				
				if _inject == -1
					array_push(items, new Folder(_folder_name, _folder_items, self))
				else
					items[_inject].add_items(_folder_items)
			}
		}
	}
	
	if _items != undefined
		add_items(_items)
}