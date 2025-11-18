#pragma once

#include <SDL3/SDL_stdinc.h>

#define MAX_OPTIONS 16

typedef enum {
	MEN_NULL,
	MEN_EXIT, // going "back" to this menu exits the game
	MEN_ERROR,
	MEN_RESULTS,
	MEN_INTRO,
	MEN_MAIN,
	MEN_SINGLEPLAYER,
	MEN_MULTIPLAYER_NOTE,
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
	bool disabled, (*disable_if)();
	void (*button)();
	void (*flip)(int);
	const char* (*format)(const char*);
	bool vivid; /// If true, won't dim the option's text even if it's disabled.

	float hover;
	MenuType enter;
	char* edit;
	const size_t edit_size;
} Option;

typedef struct {
	const char *name, *back_sound;
	void (*update)(), (*enter)(), (*leave)(MenuType next);
	bool noreturn, ghost;

	MenuType from;
	size_t option;
	float cursor;
} Menu;

void menu_init();
void update_menu();
void draw_menu();

bool set_menu(MenuType), prev_menu();
void show_error(const char*, ...);
void populate_results();
void show_results();
