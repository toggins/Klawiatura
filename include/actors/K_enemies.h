#pragma once

#include "K_game.h"

enum {
	VAL_DEAD_TYPE = VAL_CUSTOM,
};

GameActor* kill_enemy(GameActor*, Bool);

Bool maybe_hit_player(GameActor*, GameActor*);

void block_fireball(GameActor*);
void hit_fireball(GameActor*, GameActor*, int32_t);
void block_beetroot(GameActor*);
void hit_beetroot(GameActor*, GameActor*, int32_t);
