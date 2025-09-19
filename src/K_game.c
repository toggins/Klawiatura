#include "K_game.h"

const GameActorTable TAB_NULL = {0};
extern const GameActorTable TAB_PLAYER;

static const GameActorTable* const ACTORS[ACT_SIZE] = {
    [ACT_NULL] = &TAB_NULL,
    [ACT_PLAYER] = &TAB_PLAYER,
};
