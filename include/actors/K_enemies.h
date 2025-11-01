#pragma once

#include "K_game.h"

#define CUSTOM_ENEMY_FLAG(idx) CUSTOM_FLAG(idx + 1)

enum {
	VAL_ENEMY_TURN = VAL_CUSTOM,
	VAL_ENEMY_CUSTOM,

	VAL_DEAD_TYPE = VAL_CUSTOM,
};

enum {
	FLG_ENEMY_ACTIVE = CUSTOM_FLAG(0),
};

void move_enemy(GameActor*, fixed, Bool), turn_enemy(GameActor*);
GameActor* kill_enemy(GameActor*, Bool);

Bool check_stomp(GameActor*, GameActor*, int32_t), maybe_hit_player(GameActor*, GameActor*);

void hit_bump(GameActor*, GameActor*, int32_t);
Bool hit_shell(GameActor*, GameActor*);
void block_fireball(GameActor*), hit_fireball(GameActor*, GameActor*, int32_t);
void block_beetroot(GameActor*), hit_beetroot(GameActor*, GameActor*, int32_t);
void hit_hammer(GameActor*, GameActor*, int32_t);
