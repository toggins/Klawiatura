#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include "K_misc.h"

#define MAX_INPUT_DELAY 10

#define NULL_KEYBIND ((Keybind)(-1))

typedef Sint8 Keybind;
enum {
    // Game
    KB_UP,
    KB_LEFT,
    KB_DOWN,
    KB_RIGHT,

    KB_JUMP,
    KB_RUN,
    KB_FIRE,

    // Utility
    KB_CHAT,
    KB_RECORD_REPLAY,

    // Internal
    KB_PAUSE,
    KB_UI_UP,
    KB_UI_LEFT,
    KB_UI_DOWN,
    KB_UI_RIGHT,
    KB_UI_ENTER,

    KB_SIZE,
};

typedef Uint32 KeybindState;

typedef struct {
    const char* name;
    SDL_Scancode key;
    SDL_GamepadButton button;
    SDL_GamepadAxis axis;
    Bool negative;
} Binding;
extern Binding BINDS[KB_SIZE];

void input_init(), input_teardown(), input_newframe();
void input_keydown(SDL_KeyboardEvent), input_keyup(SDL_KeyboardEvent);
void input_gamepadon(SDL_GamepadDeviceEvent), input_gamepadoff(SDL_GamepadDeviceEvent);
void input_buttondown(SDL_GamepadButtonEvent), input_buttonup(SDL_GamepadButtonEvent);
void input_axis(SDL_GamepadAxisEvent);
void input_wipeout();

const char* input_device();

Bool kb_pressed(Keybind), kb_down(Keybind), kb_released(Keybind), kb_repeated(Keybind);
const char *kb_name(Keybind), *kb_label(Keybind);

void start_typing(char*, size_t, void (*)(Bool)), stop_typing();
const char* typing_what();
void input_text_input(SDL_TextInputEvent);

void start_scanning(Keybind), stop_scanning();
Keybind scanning_what();
