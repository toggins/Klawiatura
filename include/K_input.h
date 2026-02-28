#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>

#include "K_misc.h"

#define MAX_INPUT_DELAY 10L

#define NULLBIND ((Keybind)(-1L))

typedef Sint8 Keybind;
enum {
	KB_UP,
	KB_LEFT,
	KB_DOWN,
	KB_RIGHT,
	KB_JUMP,
	KB_FIRE,
	KB_RUN,

	// Utility
	KB_CHAT,

	// Non-rebindables
	KB_PAUSE,
	KB_UI_UP,
	KB_UI_LEFT,
	KB_UI_DOWN,
	KB_UI_RIGHT,
	KB_UI_ENTER,

	KB_SECRET_D,
	KB_SECRET_E,
	KB_SECRET_F,
	KB_SECRET_I,
	KB_SECRET_K,
	KB_SECRET_N,
	KB_SECRET_R,
	KB_SECRET_V,
	KB_SECRET_BAIL,

	KB_SIZE,
};

typedef Uint32 KeybindState;
typedef Sint16 KeybindValue;

typedef struct {
	const char* name;
	SDL_Scancode key;
	SDL_GamepadButton button;
	SDL_GamepadAxis axis;
	Bool negative;
} Bindings;
extern Bindings BINDS[KB_SIZE];

void input_init(), input_teardown(), input_newframe();
void input_keydown(SDL_KeyboardEvent), input_keyup(SDL_KeyboardEvent);
void input_mbdown(SDL_MouseButtonEvent), input_mbup(SDL_MouseButtonEvent);
void input_gamepadon(SDL_GamepadDeviceEvent), input_gamepadoff(SDL_GamepadDeviceEvent);
void input_buttondown(SDL_GamepadButtonEvent), input_buttonup(SDL_GamepadButtonEvent);
void input_axis(SDL_GamepadAxisEvent);
void input_wipeout();

const char* input_device();

Bool kb_pressed(Keybind), kb_down(Keybind), kb_released(Keybind), kb_repeated(Keybind);
const char* kb_label(Keybind);

Bool mb_pressed(SDL_MouseButtonFlags);
void get_cursor_pos(float* x, float* y);

void start_typing(char*, size_t), stop_typing();
const char* typing_what();
Bool typing_input_confirmed();
void input_text_input(SDL_TextInputEvent);

void start_scanning(Keybind), stop_scanning();
Keybind scanning_what();
