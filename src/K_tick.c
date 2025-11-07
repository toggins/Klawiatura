#include <SDL3/SDL_timer.h>

#include "K_input.h"
#include "K_net.h"
#include "K_tick.h"

static uint64_t last_time = 0;
static float pending_ticks = 0.f, total_ticks = 0.f, delta_ticks = 0.f;

void from_scratch() {
	last_time = SDL_GetPerformanceCounter();
	pending_ticks = total_ticks = delta_ticks = 0.f;
}

void new_frame(float ahead) {
	const uint64_t current_time = SDL_GetPerformanceCounter();
	delta_ticks = (((float)(current_time - last_time)) * ((float)TICKRATE - ahead))
	              / ((float)SDL_GetPerformanceFrequency());
	last_time = current_time;

	pending_ticks += delta_ticks;
	total_ticks += delta_ticks;
}

bool got_ticks() {
	if (pending_ticks < 1.f)
		return false;
	net_newframe();
	return true;
}

void next_tick() {
	pending_ticks -= 1.f;
	input_newframe();
}

float dt() {
	return delta_ticks;
}

float totalticks() {
	return total_ticks;
}

float pendingticks() {
	return pending_ticks;
}
