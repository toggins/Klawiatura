#include <SDL3/SDL_keyboard.h>

#include "K_input.h"
#include "K_video.h"

static KeybindState kb_then = 0, kb_now = 0;           // Tick-specific
static KeybindState kb_incoming = 0, kb_repeating = 0; // Event-specific
Bindings BINDS[KB_SIZE] = {
	[KB_UP] = {"Up",         SDL_SCANCODE_UP       },
	[KB_LEFT] = {"Left",       SDL_SCANCODE_LEFT     },
	[KB_DOWN] = {"Down",       SDL_SCANCODE_DOWN     },
	[KB_RIGHT] = {"Right",      SDL_SCANCODE_RIGHT    },
	[KB_JUMP] = {"Jump",       SDL_SCANCODE_Z        },
	[KB_FIRE] = {"Fire",       SDL_SCANCODE_X        },
	[KB_RUN] = {"Run",        SDL_SCANCODE_X        },

	[KB_PAUSE] = {"Pause",      SDL_SCANCODE_ESCAPE   },
	[KB_UI_UP] = {"UI Up",      SDL_SCANCODE_UP       },
	[KB_UI_LEFT] = {"UI Left",    SDL_SCANCODE_LEFT     },
	[KB_UI_DOWN] = {"UI Down",    SDL_SCANCODE_DOWN     },
	[KB_UI_RIGHT] = {"UI Right",   SDL_SCANCODE_RIGHT    },
	[KB_UI_ENTER] = {"UI Enter",   SDL_SCANCODE_Z        },

	[KB_KEVIN_K] = {"K",          SDL_SCANCODE_K        },
	[KB_KEVIN_E] = {"E",          SDL_SCANCODE_E        },
	[KB_KEVIN_V] = {"V",          SDL_SCANCODE_V        },
	[KB_KEVIN_I] = {"I",          SDL_SCANCODE_I        },
	[KB_KEVIN_N] = {"N",          SDL_SCANCODE_N        },
	[KB_KEVIN_BAIL] = {"Kevin Bail", SDL_SCANCODE_BACKSPACE},
};

static char* text = NULL;
static size_t* text_size = NULL;

void input_init() {}
void input_teardown() {}

void input_newframe() {
	kb_then = kb_now;
	kb_now &= kb_incoming;
	kb_repeating = 0;
}

void input_keydown(SDL_Scancode key) {
	if (typing_what() == NULL) {
		for (int i = 0; i < KB_SIZE; i++) {
			const KeybindState mask = (key == BINDS[i].key) << i;
			kb_incoming |= mask;
			kb_now |= mask;
			kb_repeating |= mask;
		}
	} else if (key == SDL_SCANCODE_BACKSPACE) {
		char* back = text + SDL_strlen(text);
		if (SDL_StepBackUTF8(text, (const char**)&back))
			*back = '\0';
	} else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_ESCAPE)
		stop_typing();
}
void input_keyup(SDL_Scancode key) {
	for (int i = 0; i < KB_SIZE; i++)
		kb_incoming &= ~((key == BINDS[i].key) << i);
}

void input_wipeout() {
	kb_then = 0;
	kb_now = 0;
	kb_incoming = 0;
	kb_repeating = 0;
}

const char* input_device() {
	// TODO
	return "Keyboard";
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

const char* kb_label(Keybind kb) {
	return SDL_GetScancodeName(BINDS[kb].key);
}

void start_typing(char* ptext, size_t* size) {
	if (window_start_typing())
		text = ptext, text_size = size;
}

void stop_typing() {
	window_stop_typing(), text = NULL, text_size = NULL;
}

const char* typing_what() {
	return (text && text_size && *text_size > 0) ? text : NULL;
}

void input_text_input(SDL_TextInputEvent event) {
	if (typing_what())
		SDL_strlcat(text, event.text, *text_size);
}
