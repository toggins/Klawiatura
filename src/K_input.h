#pragma once

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>

typedef uint8_t Keybind;
enum {
	KB_UP,
	KB_DOWN,
	KB_LEFT,
	KB_RIGHT,
	KB_EXIT,
	KB_COUNT,
};

typedef struct {
	const char* name;
	SDL_Keycode key;
} Bindings;
extern Bindings BINDS[KB_COUNT];

void input_init(), input_teardown(), input_newframe();
void input_keydown(SDL_Scancode), input_keyup(SDL_Scancode);

bool kb_pressed(Keybind), kb_down(Keybind);
int kb_axis(Keybind, Keybind);
