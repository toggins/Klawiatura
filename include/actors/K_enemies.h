#pragma once

#include "K_game.h"

enum {
    VAL_ENEMY_TURN,
    VAL_ENEMY_FRAME,
    VAL_ENEMY_CUSTOM,

    VAL_DEAD_TYPE = 0,
    VAL_DEAD_FRAME,
    VAL_DEAD_RESPAWN,
};

#define FLG_ENEMY_ACTIVE CUSTOM_FLAG(0)
#define FLG_ENEMY_FLAT CUSTOM_FLAG(1)
#define CUSTOM_ENEMY_FLAG(idx) CUSTOM_FLAG(2 + (idx))

void move_enemy(GameActor*, FVec2, Bool), turn_enemy(GameActor*);
GameActor* kill_enemy(GameActor*, GameActor*, Bool);

Bool check_stomp(GameActor*, GameActor*, Fixed, Sint32), maybe_hit_player(GameActor*, GameActor*);

void hit_bump(GameActor*, GameActor*, Sint32);
Bool hit_shell(GameActor*, GameActor*);
void block_fireball(GameActor*), hit_fireball(GameActor*, GameActor*, Sint32);
void block_beetroot(GameActor*), hit_beetroot(GameActor*, GameActor*, Sint32);

void mark_ambush_winner(GameActor*), increase_ambush(), decrease_ambush();
