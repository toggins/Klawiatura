#macro DEFAULT_LEVEL {\
	name: "",\
	texture: "",\
	next: "",\
	flags: 0,\
	size: [640, 480],\
	bounds: [0, 0, 640, 480],\
	time: -1,\
	water: 32767,\
}

global.markers = ds_list_create()
global.level = DEFAULT_LEVEL