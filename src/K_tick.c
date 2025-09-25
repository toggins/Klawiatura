#include <SDL3/SDL_timer.h>

#include "K_input.h"
#include "K_net.h"
#include "K_tick.h"

static uint64_t last_time = 0;
static float pending_ticks = 0.f, total_ticks = 0.f, delta_time = 0.f;

void from_scratch() {
	last_time = SDL_GetPerformanceCounter();
	pending_ticks = total_ticks = delta_time = 0.f;
}

void new_frame() {
	const uint64_t current_time = SDL_GetPerformanceCounter();
	delta_time = ((float)(current_time - last_time) / (float)SDL_GetPerformanceFrequency()) * (float)TICKRATE;
	last_time = current_time;

	pending_ticks += delta_time;
	total_ticks += delta_time;
}

bool got_ticks() {
	if (pending_ticks >= 1.f) {
		net_newframe();
		return true;
	}
	return false;
}

void next_tick() {
	pending_ticks -= 1.f;
	input_newframe();
}

float dt() {
	return delta_time;
}

float totalticks() {
	return total_ticks;
}
