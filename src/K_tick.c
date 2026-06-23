#include <SDL3/SDL_timer.h>

#include "K_game.h"
#include "K_input.h"
#include "K_tick.h"

static Uint64 last_time = 0;
static float pending_ticks = 0.f, total_ticks = 0.f, delta_ticks = 0.f;

void from_scratch() {
    last_time = SDL_GetTicksNS();
    pending_ticks = total_ticks = delta_ticks = 0.f;
}

void new_frame() {
    const Uint64 current_time = SDL_GetTicksNS();
    const float ahead = frames_ahead();
    delta_ticks = ((float)(current_time - last_time) * ((float)TICKRATE - SDL_clamp(ahead, 0.f, 1.f))) / 1000000000.f;
    last_time = current_time;

    pending_ticks += delta_ticks, total_ticks += delta_ticks;
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

float totalticks() {
    return total_ticks;
}

float pendingticks() {
    return pending_ticks;
}
