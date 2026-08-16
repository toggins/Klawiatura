#include <SDL3/SDL_timer.h>

#include "K_game.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_net.h"
#include "K_tick.h"

static Uint64 last_time = 0;
static float delta_ticks = 0.f, pending_ticks = 0.f;
static float screen_ticks = 0.f, ui_ticks = 0.f;

void from_scratch() {
    last_time = SDL_GetTicksNS();
    delta_ticks = pending_ticks = screen_ticks = ui_ticks = 0.f;
}

void new_frame() {
    const Uint64 current_time = SDL_GetTicksNS();
    const float ahead = frames_ahead();
    delta_ticks = ((float)(current_time - last_time) * ((float)TICKRATE - SDL_clamp(ahead, 0.f, 1.f))) / 1000000000.f;
    last_time = current_time;

    pending_ticks += delta_ticks;
    if (!screen_is_transitioning()) {
        const UI* ui = topui();
        if (ui == NULL || !(ui->flags & (is_connected() ? UIF_MEGABLOCK : (UIF_BLOCK | UIF_MEGABLOCK))))
            screen_ticks += delta_ticks;

        ui_ticks += delta_ticks;
    }
}

Bool got_ticks() {
    return pending_ticks >= 1.f;
}

void next_tick() {
    pending_ticks -= 1.f;
    input_newframe();
}

float deltaticks() {
    return delta_ticks;
}

float pendingticks() {
    return pending_ticks;
}

float screenticks() {
    return screen_ticks;
}

float uiticks() {
    return ui_ticks;
}
