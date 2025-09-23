#pragma once

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>

#define KB_VALUE_MAX 32767

typedef enum {
	KB_UP,
	KB_LEFT,
	KB_DOWN,
	KB_RIGHT,
	KB_JUMP,
	KB_FIRE,
	KB_RUN,

	// If you allow rebinding these I will pulverize you
	KB_PAUSE,
	KB_UI_UP,
	KB_UI_LEFT,
	KB_UI_DOWN,
	KB_UI_RIGHT,
	KB_UI_ENTER,

	KB_SIZE,
} Keybind;

typedef uint16_t KeybindState;
typedef int16_t KeybindValue;

typedef struct {
	const char* name;
	SDL_Keycode key;
} Bindings;
extern Bindings BINDS[KB_SIZE];

void input_init(), input_teardown(), input_newframe();
void input_keydown(SDL_Scancode), input_keyup(SDL_Scancode);

bool kb_pressed(Keybind), kb_down(Keybind);
KeybindValue kb_value(Keybind);
float kb_axis(Keybind, Keybind);
