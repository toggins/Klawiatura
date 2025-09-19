#include "K_game.h"

// NOTE: just an example on how to structure all the other actors...
//
// if you really need to use these structs outside the .c file, you're gonna have to put em in a header file next to
// this one

enum {
	VAL_PLAYER_INDEX = VAL_CUSTOM,
	VAL_PLAYER_FRAME,
	VAL_PLAYER_GROUND,
	VAL_PLAYER_SPRING,
	VAL_PLAYER_POWER,
	VAL_PLAYER_FLASH,
	VAL_PLAYER_STARMAN,
	VAL_PLAYER_STARMAN_COMBO,
	VAL_PLAYER_FIRE,
	VAL_PLAYER_WARP,
	VAL_PLAYER_WARP_STATE,
	VAL_PLAYER_PLATFORM,
};

enum {
	FLG_PLAYER_DUCK = CUSTOM_FLAG(0),
	FLG_PLAYER_JUMP = CUSTOM_FLAG(1),
	FLG_PLAYER_SWIM = CUSTOM_FLAG(2),
	FLG_PLAYER_ASCEND = CUSTOM_FLAG(3),
	FLG_PLAYER_DESCEND = CUSTOM_FLAG(4),
	FLG_PLAYER_RESPAWN = CUSTOM_FLAG(5),
	FLG_PLAYER_STOMP = CUSTOM_FLAG(6),
	FLG_PLAYER_WARP_OUT = CUSTOM_FLAG(7),
	FLG_PLAYER_DEAD = CUSTOM_FLAG(8),
};

static void tick(GameActor* actor) {
	FLAG_ON(actor, FLG_PLAYER_DEAD);
	TOGGLE_FLAG(actor, FLG_X_FLIP);
}

// don't forget to include it inside K_game.c
const GameActorTable TAB_PLAYER = {NULL, NULL, tick, NULL, NULL, NULL};
