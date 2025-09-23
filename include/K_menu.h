#pragma once

#include <SDL3/SDL_stdinc.h>

#define MAX_OPTIONS 16

typedef struct {
	const char* name;
	void (*callback)();
	bool disabled;
	void (*format)();
	float hover;
} Option;

typedef enum {
	MEN_NULL,
	MEN_INTRO,
	MEN_MAIN,
	MEN_SINGLEPLAYER,
	MEN_MULTIPLAYER,
	MEN_OPTIONS,
	MEN_CONTROLS,
	MEN_HOST_LOBBY,
	MEN_JOIN_LOBBY,
	MEN_FIND_LOBBY,
	MEN_LOBBY,
	MEN_SIZE,
} MenuType;

typedef struct {
	Option options[MAX_OPTIONS];
	MenuType from;
	size_t option;
	float cursor;
} Menu;

void start_menu(bool);
void update_menu();
void draw_menu();

void set_menu(MenuType, bool);
