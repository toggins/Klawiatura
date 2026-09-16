#pragma once

#include "K_misc.h"

#define DEFAULT_TICKRATE 50
#define MAX_TICKRATE 60

Bool got_ticks();
void from_scratch(), new_frame(), next_tick();
float deltaticks(), pendingticks(), screenticks(), uiticks();

int get_tickrate();
void set_tickrate(int);
