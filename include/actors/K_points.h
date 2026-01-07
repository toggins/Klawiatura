#pragma once

#include "K_game.h"

enum {
	VAL_POINTS = VAL_CUSTOM,
	VAL_POINTS_PLAYER,
	VAL_POINTS_TIME,
};

void give_points(GameActor*, GamePlayer*, Sint32);
