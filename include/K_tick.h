#pragma once

#include "K_misc.h"

#define TICKRATE 50

Bool got_ticks();
void from_scratch(), new_frame(), next_tick();
float deltaticks(), totalticks(), pendingticks();
