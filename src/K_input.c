#include <SDL3/SDL_keyboard.h>

#include "K_input.h"

static KeybindState kb_then = 0, kb_now = 0;           // Tick-specific
static KeybindState kb_incoming = 0, kb_repeating = 0; // Event-specific
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
void input_newframe() {
	kb_then = kb_now;
	kb_now &= kb_incoming;
	kb_repeating = 0;
}

void input_keydown(SDL_Scancode key) {
	for (int i = 0; i < KB_SIZE; i++) {
		const KeybindState mask = (key == BINDS[i].key) << i;
		kb_incoming |= mask;
		kb_now |= mask;
		kb_repeating |= mask;
	}
}
void input_keyup(SDL_Scancode key) {
	for (int i = 0; i < KB_SIZE; i++)
		kb_incoming &= ~((key == BINDS[i].key) << i);
}

#define CHECK_KB(table, kb) (((table) & (1 << (kb))) != 0)

bool kb_pressed(Keybind kb) {
	return CHECK_KB(kb_now, kb) && !CHECK_KB(kb_then, kb);
}

bool kb_down(Keybind kb) {
	return CHECK_KB(kb_now, kb);
}

bool kb_repeated(Keybind kb) {
	return CHECK_KB(kb_now, kb) && CHECK_KB(kb_repeating, kb);
}

bool kb_released(Keybind kb) {
	return !CHECK_KB(kb_now, kb) && CHECK_KB(kb_then, kb);
}

KeybindValue kb_value(Keybind kb) {
	return CHECK_KB(kb_now, kb) ? KB_VALUE_MAX : 0;
}

float kb_axis(Keybind neg, Keybind pos) {
	return (float)(kb_value(pos) - kb_value(neg)) / (float)KB_VALUE_MAX;
}
