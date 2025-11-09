#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>

#define KB_VALUE_MAX 32767
#define MAX_INPUT_DELAY 10L

#define NULLBIND ((Keybind)(-1))

typedef int8_t Keybind;
enum {
	// Gaming 🔥🔥🔥
	KB_UP,
	KB_LEFT,
	KB_DOWN,
	KB_RIGHT,
	KB_JUMP,
	KB_FIRE,
	KB_RUN,

	// Utility
	KB_CHAT,
	KB_RESTART,

	// If you allow rebinding these I will pulverize you
	KB_PAUSE,
	KB_UI_UP,
	KB_UI_LEFT,
	KB_UI_DOWN,
	KB_UI_RIGHT,
	KB_UI_ENTER,

	// If you allow rebinding these then that's not Kevin
	KB_KEVIN_K,
	KB_KEVIN_E,
	KB_KEVIN_V,
	KB_KEVIN_I,
	KB_KEVIN_N,
	KB_KEVIN_BAIL,

	KB_SIZE,
};

typedef uint32_t KeybindState;
typedef int16_t KeybindValue;

typedef struct {
	const char* name;
	SDL_Keycode key;
} Bindings;
extern Bindings BINDS[KB_SIZE];

void input_init(), input_teardown(), input_newframe();
void input_keydown(SDL_Scancode), input_keyup(SDL_Scancode);
void input_wipeout();

const char* input_device();

bool kb_pressed(Keybind), kb_down(Keybind), kb_released(Keybind), kb_repeated(Keybind);
KeybindValue kb_value(Keybind);
float kb_axis(Keybind, Keybind);
const char* kb_label(Keybind);

void start_typing(char*, size_t);
void stop_typing();
const char* typing_what();
void input_text_input(SDL_TextInputEvent);

void start_scanning(Keybind);
void stop_scanning();
Keybind scanning_what();
