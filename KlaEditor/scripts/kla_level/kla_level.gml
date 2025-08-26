#macro MAJOR_FILE_VERSION 0
#macro MINOR_FILE_VERSION 0

#macro DEFAULT_LEVEL {\
	name: "",\
	texture: "",\
	next: "",\
	track: ["", ""],\
	flags: 0,\
	size: [640, 480],\
	bounds: [0, 0, 640, 480],\
	time: -1,\
	water: 32767,\
	hazard: 0,\
}

enum LevelFlags {
	LAVA = 1 << 3,
	HARDCORE = 1 << 4,
	SPIKES = 1 << 5,
	FUNNY = 1 << 6,
	LOST = 1 << 7,
	KEVIN = 1 << 8,
}

global.markers = ds_list_create()
global.level = DEFAULT_LEVEL