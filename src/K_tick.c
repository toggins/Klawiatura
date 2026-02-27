#include <SDL3/SDL_timer.h>

#include "K_chat.h"
#include "K_input.h"
#include "K_net.h"
#include "K_tick.h"

static Uint64 last_time = 0;
static float pending_ticks = 0.f, total_ticks = 0.f, delta_ticks = 0.f;

void from_scratch() {
	last_time = SDL_GetTicksNS();
	pending_ticks = total_ticks = delta_ticks = 0.f;
}

void new_frame(float ahead) {
	const Uint64 current_time = SDL_GetTicksNS();
	delta_ticks = ((float)(current_time - last_time) * ((float)TICKRATE - ahead)) / 1000000000.f;
	last_time = current_time;

	pending_ticks += delta_ticks;
	total_ticks += delta_ticks;

	tick_chat_hist();
}

Bool got_ticks() {
	if (pending_ticks < 1.f)
		return FALSE;
	net_newframe();
	return TRUE;
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
