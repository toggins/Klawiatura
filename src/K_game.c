#include "K_game.h"

const struct GameActorInfo K_NULL = {0};
extern const struct GameActorInfo K_PLAYER;

static const struct GameActorInfo* ACTORS[ACT_SIZE] = {
    [ACT_NULL] = &K_NULL,
    [ACT_PLAYER] = &K_PLAYER,
};
