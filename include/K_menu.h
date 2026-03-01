#pragma once

#include <SDL3/SDL_stdinc.h>

#include "K_input.h"

#define MAX_OPTIONS 16L
#define MAX_SECRET_INPUTS 5L

typedef enum {
	MEN_NULL,
	MEN_EXIT, // going "back" to this menu exits the game
	MEN_ERROR,
	MEN_RESULTS,
	MEN_INTRO,
	MEN_MAIN,
	MEN_LEVEL_SELECT,
	MEN_SINGLEPLAYER,
	MEN_MULTIPLAYER_NOTE,
	MEN_MULTIPLAYER,
	MEN_OPTIONS,
	MEN_CONTROLS,
	MEN_HOST_LOBBY,
	MEN_LOBBY_ID,
	MEN_FIND_LOBBY,
	MEN_JOINING_LOBBY,
	MEN_LOBBY,
	// "ingame" menu marker since they're handled a bit differently
	MEN_INGAME,
	MEN_INGAME_PLAYING = MEN_INGAME,
	MEN_INGAME_PAUSE,
	MEN_SIZE,
} MenuType;

typedef struct {
	const char* name;
	Bool disabled, (*disable_if)();
	void (*button)();
	void (*flip)(int);
	const char* (*format)(const char*);
	Bool vivid; /// If true, won't dim the option's text even if it's disabled.

	float hover;
	MenuType enter;
	char* edit;
	const size_t edit_size;
} Option;

typedef struct {
	const char *name, *back_sound;
	void (*update)();
	// don't call `set_menu` here please
	void (*enter)();
	// don't call `set_menu` here please
	void (*leave)(MenuType next);
	Bool noreturn, ghost;

	MenuType from;
	int option;
	float cursor;
} Menu;

typedef enum {
	SECR_KEVIN,
	SECR_FRED,
	SECR_SIZE,
} SecretType;

typedef struct {
	const char* name;
	Keybind inputs[MAX_SECRET_INPUTS + 1];

	Bool* cmd;
	const char *sound, *track;

	size_t state;
	Uint8 type_time;
	Bool active;
} Secret;

void load_menu();

void menu_init();
void update_menu(), draw_menu();

Bool set_menu(MenuType), prev_menu();
void show_error(const char*, ...);

void populate_results(), show_results();
const char* who_is_winner(int);
