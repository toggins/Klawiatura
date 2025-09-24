#pragma once

#include <SDL3/SDL_stdinc.h>

#define MAX_OPTIONS 16

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
	MEN_JOINING_LOBBY,
	MEN_LOBBY,
	MEN_SIZE,
} MenuType;

typedef struct {
	const char* name;
	bool disabled;
	void (*callback)();
	const char* (*format)();
	float hover;
	MenuType enter;
} Option;

typedef struct {
	const char* name;
	bool noreturn;

	MenuType from;
	size_t option;
	float cursor;
} Menu;

void start_menu(bool);
void update_menu();
void draw_menu();

bool set_menu(MenuType);
