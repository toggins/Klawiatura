#include <SDL3/SDL_keyboard.h>

#include "K_input.h"

static Keybind kb_now = 0, kb_then = 0;
Bindings BINDS[KB_COUNT] = {
	[KB_EXIT] = {"Exit", SDL_SCANCODE_ESCAPE},
};

void input_init() {}
void input_teardown() {}

void input_newframe() {
	kb_then = kb_now;
}

#define KB_SET_IF(cond)                                                                                                \
	for (int i = 0; i < KB_COUNT; i++)                                                                             \
		kb_now |= (cond) << i;
#define KB_UNSET_IF(cond)                                                                                              \
	for (int i = 0; i < KB_COUNT; i++)                                                                             \
		kb_now &= ~((cond) << i);

void input_keydown(SDL_Scancode key) {
	KB_SET_IF(key == BINDS[i].key);
}
void input_keyup(SDL_Scancode key) {
	KB_UNSET_IF(key == BINDS[i].key);
}

#define CHECK_KB(table, kb) (((table) & (1 << (kb))) != 0)

bool kb_pressed(Keybind kb) {
	return !CHECK_KB(kb_now, kb) && CHECK_KB(kb_then, kb);
}

bool kb_down(Keybind kb) {
	return CHECK_KB(kb_now, kb);
}

int kb_axis(Keybind left, Keybind right) {
	return ((int)kb_down(right)) - ((int)kb_down(left));
}
