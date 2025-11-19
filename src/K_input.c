#include <SDL3/SDL_keyboard.h>

#include "K_file.h"
#include "K_input.h"
#include "K_log.h"
#include "K_video.h"

static KeybindState kb_then = 0, kb_now = 0;           // Tick-specific
static KeybindState kb_incoming = 0, kb_repeating = 0; // Event-specific

#define KEY(x) .key = SDL_SCANCODE_##x
#define NO_KEY .key = SDL_SCANCODE_UNKNOWN
#define BUTTON(x) .button = SDL_GAMEPAD_BUTTON_##x
#define NO_BUTTON .button = SDL_GAMEPAD_BUTTON_INVALID
#define AXIS(x) .axis = SDL_GAMEPAD_AXIS_##x
#define NO_AXIS .axis = SDL_GAMEPAD_AXIS_INVALID
#define NO_GAMEPAD NO_BUTTON, NO_AXIS
#define NEGATIVE .negative = true
Bindings BINDS[KB_SIZE] = {
	[KB_UP] = {"Up", KEY(UP), BUTTON(DPAD_UP), AXIS(LEFTY), NEGATIVE},
	[KB_LEFT] = {"Left", KEY(LEFT), BUTTON(DPAD_LEFT), AXIS(LEFTX), NEGATIVE},
	[KB_DOWN] = {"Down", KEY(DOWN), BUTTON(DPAD_DOWN), AXIS(LEFTY)},
	[KB_RIGHT] = {"Right", KEY(RIGHT), BUTTON(DPAD_RIGHT), AXIS(LEFTX)},
	[KB_JUMP] = {"Jump", KEY(Z), BUTTON(SOUTH), NO_AXIS},
	[KB_FIRE] = {"Fire", KEY(X), NO_BUTTON, AXIS(RIGHT_TRIGGER)},
	[KB_RUN] = {"Run", KEY(X), NO_BUTTON, AXIS(RIGHT_TRIGGER)},

	[KB_CHAT] = {"Open Chat", KEY(T), NO_GAMEPAD},

	[KB_PAUSE] = {"Pause", KEY(ESCAPE), BUTTON(START), NO_AXIS},
	[KB_UI_UP] = {"UI Up", KEY(UP), BUTTON(DPAD_UP), AXIS(LEFTY), NEGATIVE},
	[KB_UI_LEFT] = {"UI Left", KEY(LEFT), BUTTON(DPAD_LEFT), AXIS(LEFTX), NEGATIVE},
	[KB_UI_DOWN] = {"UI Down", KEY(DOWN), BUTTON(DPAD_DOWN), AXIS(LEFTY)},
	[KB_UI_RIGHT] = {"UI Right", KEY(RIGHT), BUTTON(DPAD_RIGHT), AXIS(LEFTX)},
	[KB_UI_ENTER] = {"UI Enter", KEY(RETURN), BUTTON(SOUTH), NO_AXIS},

	[KB_KEVIN_K] = {"K", KEY(K), NO_GAMEPAD},
	[KB_KEVIN_E] = {"E", KEY(E), NO_GAMEPAD},
	[KB_KEVIN_V] = {"V", KEY(V), NO_GAMEPAD},
	[KB_KEVIN_I] = {"I", KEY(I), NO_GAMEPAD},
	[KB_KEVIN_N] = {"N", KEY(N), NO_GAMEPAD},
	[KB_KEVIN_BAIL] = {"Kevin Bail", KEY(BACKSPACE), BUTTON(BACK), NO_AXIS},
};
#undef KEY
#undef BUTTON
#undef NO_BUTTON
#undef AXIS
#undef NO_AXIS
#undef NO_GAMEPAD
#undef NEGATIVE

static SDL_JoystickID cur_controller = 0;

static char* text = NULL;
static size_t text_size = 0;

static SDL_JoystickID scanning_for = 0;
static Keybind scanning = NULLBIND;

void input_init() {
	if (SDL_AddGamepadMappingsFromFile(find_data_file("gamecontrollerdb.txt", NULL)) < 0)
		WARN("Failed to load gamecontrollerdb.txt: %s", SDL_GetError());
}

void input_teardown() {}

void input_newframe() {
	kb_then = kb_now;
	kb_now &= kb_incoming;
	kb_repeating = 0;
}

void input_keydown(SDL_KeyboardEvent event) {
	cur_controller = 0;

	if (event.mod & SDL_KMOD_ALT && event.scancode == SDL_SCANCODE_RETURN) {
		set_fullscreen(!get_fullscreen());
	} else if (scanning_what() != NULLBIND) {
		if (event.scancode == SDL_SCANCODE_ESCAPE)
			stop_scanning();
		else if (!scanning_for) {
			BINDS[scanning].key = event.scancode;
			stop_scanning();
		}
	} else if (typing_what() == NULL) {
		for (int i = 0; i < KB_SIZE; i++) {
			const KeybindState mask = (event.scancode == BINDS[i].key) << i;
			kb_incoming |= mask;
			kb_now |= mask;
			kb_repeating |= mask;
		}
	} else if (event.scancode == SDL_SCANCODE_BACKSPACE) {
		char* back = text + SDL_strlen(text);
		if (SDL_StepBackUTF8(text, (const char**)&back))
			*back = '\0';
	} else if (event.scancode == SDL_SCANCODE_RETURN || event.scancode == SDL_SCANCODE_ESCAPE) {
		stop_typing();
	}
}

void input_keyup(SDL_KeyboardEvent event) {
	for (int i = 0; i < KB_SIZE; i++)
		kb_incoming &= ~((event.scancode == BINDS[i].key) << i);
}

void input_gamepadon(SDL_GamepadDeviceEvent event) {
	struct SDL_Gamepad* gamepad = SDL_OpenGamepad(event.which);
	if (gamepad != NULL)
		cur_controller = SDL_GetGamepadID(gamepad);
}

void input_gamepadoff(SDL_GamepadDeviceEvent event) {
	SDL_CloseGamepad(SDL_GetGamepadFromID(event.which));
	if (cur_controller == event.which)
		cur_controller = 0;
	if (scanning_for == event.which)
		stop_scanning();
}

void input_buttondown(SDL_GamepadButtonEvent event) {
	cur_controller = event.which;

	if (scanning_what() != NULLBIND) {
		if (event.button == SDL_GAMEPAD_BUTTON_BACK)
			stop_scanning();
		else if (scanning_for == event.which) {
			BINDS[scanning].button = event.button;
			stop_scanning();
		}
	} else if (typing_what() == NULL) {
		for (int i = 0; i < KB_SIZE; i++) {
			const KeybindState mask = (event.button == BINDS[i].button) << i;
			kb_incoming |= mask;
			kb_now |= mask;
			kb_repeating |= mask;
		}
	}
}

void input_buttonup(SDL_GamepadButtonEvent event) {
	for (int i = 0; i < KB_SIZE; i++)
		kb_incoming &= ~((event.button == BINDS[i].button) << i);
}

void input_axis(SDL_GamepadAxisEvent event) {
	cur_controller = event.which;

	if (scanning_what() != NULLBIND) {
		if (scanning_for == event.which && event.value > 127) {
			BINDS[scanning].axis = event.axis;
			BINDS[scanning].negative = event.value < 0;
			stop_scanning();
		}
	} else if (typing_what() == NULL) {
		for (int i = 0; i < KB_SIZE; i++) {
			const KeybindState mask = (event.axis == BINDS[i].axis
							  && ((BINDS[i].negative && event.value < -128)
								  || (!BINDS[i].negative && event.value > 127)))
			                          << i;
			kb_incoming |= mask;
			kb_now |= mask;
			kb_repeating |= mask;
		}
	}

	for (int i = 0; i < KB_SIZE; i++)
		kb_incoming &= ~((event.axis == BINDS[i].axis
					 && ((BINDS[i].negative && event.value >= -128)
						 || (!BINDS[i].negative && event.value <= 127)))
				 << i);
}

void input_wipeout() {
	kb_then = 0;
	kb_now = 0;
	kb_incoming = 0;
	kb_repeating = 0;
}

const char* input_device() {
	const SDL_JoystickID target = (scanning == NULLBIND) ? cur_controller : scanning_for;
	return target ? SDL_GetGamepadNameForID(target) : "Keyboard";
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
	const SDL_JoystickID target = (scanning == NULLBIND) ? cur_controller : scanning_for;
	if (!target) {
		if (BINDS[kb].key != SDL_SCANCODE_UNKNOWN)
			return SDL_GetScancodeName(BINDS[kb].key);
		goto not_bound;
	}

	if (BINDS[kb].axis != SDL_GAMEPAD_AXIS_INVALID)
		return SDL_GetGamepadStringForAxis(BINDS[kb].axis);
	if (BINDS[kb].button != SDL_GAMEPAD_BUTTON_INVALID)
		return SDL_GetGamepadStringForButton(BINDS[kb].button);

not_bound:
	return "<Not bound>";
}

void start_typing(char* ptext, size_t size) {
	stop_typing();
	if (window_start_text_input())
		text = ptext, text_size = size;
}

void stop_typing() {
	window_stop_text_input(), text = NULL, text_size = 0;
}

const char* typing_what() {
	return (text && text_size) ? text : NULL;
}

void input_text_input(SDL_TextInputEvent event) {
	if (typing_what())
		SDL_strlcat(text, event.text, text_size);
}

void start_scanning(Keybind kb) {
	stop_scanning();
	scanning = (Keybind)((kb >= 0 && kb < KB_SIZE) ? kb : NULLBIND);
	scanning_for = cur_controller;
}

void stop_scanning() {
	scanning = NULLBIND;
	scanning_for = 0;
	input_wipeout();
}

Keybind scanning_what() {
	return scanning;
}
