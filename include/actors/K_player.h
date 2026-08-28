#pragma once

#include "K_game.h"

/* ======
   PLAYER
   ====== */

enum {
    VAL_PLAYER_ANIMATION,
    VAL_PLAYER_FRAME,
    VAL_PLAYER_WARP,
    VAL_PLAYER_WARP_STATE,
    VAL_PLAYER_WARP_OUT_ANGLE,
    VAL_PLAYER_FLASH,
    VAL_PLAYER_STARMAN,
    VAL_PLAYER_STARMAN_COMBO,
    VAL_PLAYER_SPRING,

    VAL_PLAYER_EFFECT_CHARACTER = 0,
    VAL_PLAYER_EFFECT_POWERUP,
    VAL_PLAYER_EFFECT_FRAME,
    VAL_PLAYER_EFFECT_ALPHA,

    VAL_PLAYER_DEAD = 0,
};

#define FLG_PLAYER_JUMP CUSTOM_FLAG(0)
#define FLG_PLAYER_DUCK CUSTOM_FLAG(1)
#define FLG_PLAYER_TOUCHED_WATER CUSTOM_FLAG(2)
#define FLG_PLAYER_WARP_OUT CUSTOM_FLAG(3)
#define FLG_PLAYER_STOMP CUSTOM_FLAG(4)
#define FLG_PLAYER_DESCEND CUSTOM_FLAG(5)

Bool hit_player(GameActor*);
void kill_player(GameActor*);
void grow_player(GameActor*, GameActor*, PlayerPowerup);
void player_starman(GameActor*, GameActor*);
