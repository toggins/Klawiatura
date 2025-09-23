#include <SDL3/SDL_keyboard.h>

#include "K_input.h"

static KeybindState kb_now = 0, kb_already_pressed = 0;
Bindings BINDS[KB_SIZE] = {
	[KB_UP] = {"Up",       SDL_SCANCODE_UP    },
	[KB_LEFT] = {"Left",     SDL_SCANCODE_LEFT  },
	[KB_DOWN] = {"Down",     SDL_SCANCODE_DOWN  },
	[KB_RIGHT] = {"Right",    SDL_SCANCODE_RIGHT },
	[KB_JUMP] = {"Jump",     SDL_SCANCODE_Z     },
	[KB_FIRE] = {"Fire",     SDL_SCANCODE_X     },
	[KB_RUN] = {"Run",      SDL_SCANCODE_C     },

	[KB_PAUSE] = {"Pause",    SDL_SCANCODE_ESCAPE},
	[KB_UI_UP] = {"UI Up",    SDL_SCANCODE_UP    },
	[KB_UI_LEFT] = {"UI Left",  SDL_SCANCODE_LEFT  },
	[KB_UI_DOWN] = {"UI Down",  SDL_SCANCODE_DOWN  },
	[KB_UI_RIGHT] = {"UI Right", SDL_SCANCODE_RIGHT },
	[KB_UI_ENTER] = {"UI Enter", SDL_SCANCODE_Z     },
};

void input_init() {}
void input_teardown() {}
void input_newframe() {}

void input_keydown(SDL_Scancode key) {
	for (int i = 0; i < KB_SIZE; i++)
		kb_now |= (key == BINDS[i].key) << i;
}
void input_keyup(SDL_Scancode key) {
	for (int i = 0; i < KB_SIZE; i++) {
		const KeybindState mask = ~((key == BINDS[i].key) << i);
		kb_now &= mask;
		kb_already_pressed &= mask;
	}
}

#define CHECK_KB(table, kb) (((table) & (1 << (kb))) != 0)

bool kb_pressed(Keybind kb) {
	if (CHECK_KB(kb_now, kb) && !CHECK_KB(kb_already_pressed, kb)) {
		kb_already_pressed |= 1 << kb;
		return true;
	}
	return false;
}

bool kb_down(Keybind kb) {
	return CHECK_KB(kb_now, kb);
}

KeybindValue kb_value(Keybind kb) {
	return CHECK_KB(kb_now, kb) ? KB_VALUE_MAX : 0;
}

float kb_axis(Keybind neg, Keybind pos) {
	return (float)(kb_value(pos) - kb_value(neg)) / (float)KB_VALUE_MAX;
}
