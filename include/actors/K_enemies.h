#pragma once

#include "K_game.h"

enum {
	VAL_DEAD_TYPE = VAL_CUSTOM,
};

GameActor* kill_enemy(GameActor*, Bool);
void block_fireball(GameActor*);
