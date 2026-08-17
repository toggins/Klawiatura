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
};

#define FLG_PLAYER_JUMP CUSTOM_FLAG(0)
#define FLG_PLAYER_DUCK CUSTOM_FLAG(1)
#define FLG_PLAYER_TOUCHED_WATER CUSTOM_FLAG(2)
#define FLG_PLAYER_WARP_OUT CUSTOM_FLAG(3)

/* =============
   PLAYER EFFECT
   ============= */

enum {
    VAL_PLAYER_EFFECT_CHARACTER,
    VAL_PLAYER_EFFECT_POWERUP,
    VAL_PLAYER_EFFECT_FRAME,
    VAL_PLAYER_EFFECT_ALPHA,
};

/* ===========
   DEAD PLAYER
   =========== */

enum {
    VAL_PLAYER_DEAD,
};

void kill_player(GameActor*);
