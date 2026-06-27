#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_platform_defines.h>

#include "K_chat.h"
#include "K_file.h"
#include "K_input.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_video.h"

#define KEY(x) .key = SDL_SCANCODE_##x
#define NO_KEY .key = SDL_SCANCODE_UNKNOWN
#define BUTTON(x) .button = SDL_GAMEPAD_BUTTON_##x
#define NO_BUTTON .button = SDL_GAMEPAD_BUTTON_INVALID
#define AXIS(x) .axis = SDL_GAMEPAD_AXIS_##x
#define NO_AXIS .axis = SDL_GAMEPAD_AXIS_INVALID
#define NO_GAMEPAD NO_BUTTON, NO_AXIS
#define NOTHING NO_KEY, NO_GAMEPAD
#define NEGATIVE .negative = TRUE
Binding BINDS[KB_SIZE] = {
    [KB_UP] = {"kb_up", KEY(UP), BUTTON(DPAD_UP), AXIS(LEFTY), NEGATIVE},
    [KB_LEFT] = {"kb_left", KEY(LEFT), BUTTON(DPAD_LEFT), AXIS(LEFTX), NEGATIVE},
    [KB_DOWN] = {"kb_down", KEY(DOWN), BUTTON(DPAD_DOWN), AXIS(LEFTY)},
    [KB_RIGHT] = {"kb_right", KEY(RIGHT), BUTTON(DPAD_RIGHT), AXIS(LEFTX)},

    [KB_JUMP] = {"kb_jump", KEY(Z), BUTTON(SOUTH), NO_AXIS},
    [KB_RUN] = {"kb_run", KEY(X), NO_BUTTON, AXIS(RIGHT_TRIGGER)},
    [KB_FIRE] = {"kb_fire", KEY(X), NO_BUTTON, AXIS(RIGHT_TRIGGER)},

    [KB_CHAT] = {"kb_chat", KEY(T), NO_GAMEPAD},
    [KB_RECORD_REPLAY] = {"kb_record_replay", KEY(F9), NO_GAMEPAD},

    [KB_PAUSE] = {"kb_pause", KEY(ESCAPE), BUTTON(START), NO_AXIS},
    [KB_UI_UP] = {"kb_ui_up", KEY(UP), BUTTON(DPAD_UP), AXIS(LEFTY), NEGATIVE},
    [KB_UI_LEFT] = {"kb_ui_left", KEY(LEFT), BUTTON(DPAD_LEFT), AXIS(LEFTX), NEGATIVE},
    [KB_UI_DOWN] = {"kb_ui_down", KEY(DOWN), BUTTON(DPAD_DOWN), AXIS(LEFTY)},
    [KB_UI_RIGHT] = {"kb_ui_right", KEY(RIGHT), BUTTON(DPAD_RIGHT), AXIS(LEFTX)},
    [KB_UI_ENTER] = {"kb_ui_enter", KEY(RETURN), BUTTON(SOUTH), NO_AXIS},
};
#undef KEY
#undef BUTTON
#undef NO_BUTTON
#undef AXIS
#undef NO_AXIS
#undef NO_GAMEPAD
#undef NOTHING
#undef NEGATIVE

typedef struct {
    SDL_JoystickID device;
    KeybindState then, now, incoming, repeating;
} InputState;
static InputState input_state = {0};

static struct {
    char* ptr;
    size_t size;
    void (*submit)(Bool);
} typing = {0};

static struct {
    SDL_JoystickID device;
    Keybind kb;
} scanning = {.kb = NULL_KEYBIND};

void input_init() {
    if (SDL_AddGamepadMappingsFromIO(stream_base_file("gamecontrollerdb.txt"), TRUE) < 0)
        WARN("Failed to load gamecontrollerdb.txt: %s", SDL_GetError());
}

void input_teardown() {}

void input_newframe() {
    input_state.then = input_state.now;
    input_state.now &= input_state.incoming;
    input_state.repeating = 0;
}

static void stop_typing_fr(Bool confirmed) {
    extern SDL_Window* WINDOW;
    SDL_StopTextInput(WINDOW);

    if (typing.ptr == NULL)
        return;
    if (typing.submit != NULL)
        typing.submit(confirmed);
    typing.ptr = NULL;
    typing.size = 0;
}

void input_keydown(SDL_KeyboardEvent event) {
#ifndef SDL_PLATFORM_EMSCRIPTEN
    if ((event.mod & SDL_KMOD_ALT) && event.scancode == SDL_SCANCODE_RETURN) {
        set_fullscreen(!get_fullscreen());
        return;
    }
#endif

    if (scanning_what() != NULL_KEYBIND) {
        if (event.scancode == SDL_SCANCODE_ESCAPE) {
            stop_scanning();
        } else if (scanning.device <= 0) {
            BINDS[scanning.kb].key = event.scancode;
            stop_scanning();
        }

        return;
    }

    if (typing_what() != NULL) {
        switch (event.scancode) {
        default:
            break;

        case SDL_SCANCODE_C: {
            if ((event.mod & SDL_KMOD_CTRL) && SDL_strnlen(typing.ptr, typing.size) > 0)
                SDL_SetClipboardText(typing.ptr);

            break;
        }

        case SDL_SCANCODE_V: {
            if ((event.mod & SDL_KMOD_CTRL) && SDL_HasClipboardText()) {
                char *text = SDL_GetClipboardText(), *newline = text;
                while ((newline = SDL_strpbrk(newline, "\r\n")))
                    *newline = ' ';

                SDL_strlcat(typing.ptr, text, typing.size);
                SDL_free(text);
            }

            break;
        }

        case SDL_SCANCODE_BACKSPACE: {
            char* back = typing.ptr + SDL_strlen(typing.ptr);
            if (SDL_StepBackUTF8(typing.ptr, (const char**)&back))
                *back = '\0';

            break;
        }

        case SDL_SCANCODE_RETURN: {
            stop_typing_fr(TRUE);
            break;
        }

        case SDL_SCANCODE_ESCAPE: {
            stop_typing_fr(FALSE);
            break;
        }
        }

        return;
    }

    for (Keybind kb = 0; kb < (Keybind)KB_SIZE; kb++) {
        if (event.scancode != BINDS[kb].key)
            continue;

        const KeybindState mask = 1 << kb;
        input_state.incoming |= mask;
        input_state.now |= mask;
        input_state.repeating |= mask;
    }
}

void input_keyup(SDL_KeyboardEvent event) {
    for (Keybind kb = 0; kb < (Keybind)KB_SIZE; kb++)
        input_state.incoming &= ~((event.scancode == BINDS[kb].key) << kb);
}

void input_gamepadon(SDL_GamepadDeviceEvent event) {
    SDL_Gamepad* gamepad = SDL_OpenGamepad(event.which);
    if (gamepad != NULL && input_state.device <= 0)
        input_state.device = SDL_GetGamepadID(gamepad);
}

void input_gamepadoff(SDL_GamepadDeviceEvent event) {
    SDL_CloseGamepad(SDL_GetGamepadFromID(event.which));

    if (input_state.device == event.which)
        input_state.device = 0;

    if (scanning.device == event.which)
        stop_scanning();
}

void input_buttondown(SDL_GamepadButtonEvent event) {
    if (scanning_what() != NULL_KEYBIND) {
        if (event.button == SDL_GAMEPAD_BUTTON_BACK) {
            stop_scanning();
        } else if (scanning.device == event.which) {
            BINDS[scanning.kb].button = event.button;
            stop_scanning();
        }

        return;
    }

    if (input_state.device != event.which)
        return;

    for (Keybind kb = 0; kb < (Keybind)KB_SIZE; kb++) {
        if (event.button != BINDS[kb].button)
            continue;

        const KeybindState mask = 1 << kb;
        input_state.incoming |= mask;
        input_state.now |= mask;
        input_state.repeating |= mask;
    }
}

void input_buttonup(SDL_GamepadButtonEvent event) {
    if (input_state.device == event.which)
        for (Keybind i = 0; i < (Keybind)KB_SIZE; i++)
            input_state.incoming &= ~((event.button == BINDS[i].button) << i);
}

void input_axis(SDL_GamepadAxisEvent event) {
    if (scanning_what() != NULL_KEYBIND) {
        if (scanning.device == event.which && (event.value < -8192 || event.value > 8192)) {
            BINDS[scanning.kb].button = event.axis;
            BINDS[scanning.kb].negative = event.value < 0;
            stop_scanning();
        }

        return;
    }

    if (input_state.device != event.which)
        return;

    for (Keybind i = 0; i < (Keybind)KB_SIZE; i++) {
        const KeybindState mask
            = (event.axis == BINDS[i].axis
                  && ((BINDS[i].negative && event.value < -8192) || (!BINDS[i].negative && event.value > 8192)))
              << i;
        input_state.incoming |= mask;
        input_state.now |= mask;
        input_state.repeating |= mask;
    }

    for (Keybind i = 0; i < (Keybind)KB_SIZE; i++) {
        input_state.incoming
            &= ~((event.axis == BINDS[i].axis
                     && ((BINDS[i].negative && event.value >= -8192) || (!BINDS[i].negative && event.value <= 8192)))
                 << i);
    }
}

void input_wipeout() {
    input_state.then = input_state.now = input_state.incoming = input_state.repeating = 0;

    if (!typing_in_chat())
        stop_typing();

    if (scanning_what() != NULL_KEYBIND)
        stop_scanning();
}

const char* input_device() {
    const char* dname = SDL_GetGamepadNameForID(input_state.device);
    return (dname == NULL) ? LFMT("val_keyboard") : dname;
}

#define CHECK_KB(table, kb) (((table) & (1 << (kb))) != 0)

Bool kb_pressed(Keybind kb) {
    return CHECK_KB(input_state.now, kb) && !CHECK_KB(input_state.then, kb);
}

Bool kb_down(Keybind kb) {
    return CHECK_KB(input_state.now, kb);
}

Bool kb_repeated(Keybind kb) {
    return CHECK_KB(input_state.now, kb) && CHECK_KB(input_state.repeating, kb);
}

Bool kb_released(Keybind kb) {
    return !CHECK_KB(input_state.now, kb) && CHECK_KB(input_state.then, kb);
}

const char* kb_name(Keybind kb) {
    return BINDS[kb].name;
}

const char* kb_label(Keybind kb) {
    const SDL_Scancode key = BINDS[kb].key;
    return (key == SDL_SCANCODE_UNKNOWN) ? LFMT("val_not_bound") : SDL_GetScancodeName(key);
}

void start_typing(char* ptext, size_t size, void (*submit)(Bool)) {
    if (ptext == NULL || size <= 0)
        return;

    stop_typing();

    extern SDL_Window* WINDOW;
    if (!SDL_StartTextInput(WINDOW))
        return;

    typing.ptr = ptext;
    typing.size = size;
    typing.submit = submit;
}

void stop_typing() {
    stop_typing_fr(FALSE);
}

const char* typing_what() {
    return (typing.ptr != NULL && typing.size > 0) ? typing.ptr : NULL;
}

void input_text_input(SDL_TextInputEvent event) {
    if (typing_what() != NULL)
        SDL_strlcat(typing.ptr, event.text, typing.size);
}

void start_scanning(Keybind kb) {
    if (kb < 0 || kb >= KB_SIZE)
        return;

    stop_typing();

    scanning.device = input_state.device;
    scanning.kb = kb;
}

void stop_scanning() {
    scanning.device = 0;
    scanning.kb = NULL_KEYBIND;
}

Keybind scanning_what() {
    return scanning.kb;
}
