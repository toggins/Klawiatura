#include "K_game.h"

#define ACTOR(ident)                                                                                                   \
    extern const ActorTable TAB_##ident;                                                                               \
    ACTORS[ACT_##ident] = &TAB_##ident;

void POPULATE_ACTORS_TABLE() {
    extern const ActorTable* ACTORS[ACT_SIZE];

    static const ActorTable TAB_NULL = {NULL};
    ACTORS[ACT_NULL] = &TAB_NULL;

    ACTOR(SOLID);
    ACTOR(SOLID_TOP);
    ACTOR(SOLID_SLOPE);
    ACTOR(PLAYER_SPAWN);
    ACTOR(PLAYER);
    ACTOR(PLAYER_EFFECT);
    ACTOR(PLAYER_DEAD);
    ACTOR(POINTS);
    ACTOR(WARP);
    ACTOR(CHECKPOINT);
    ACTOR(CHECKPOINT_EFFECT);
    ACTOR(GOAL_BAR);
    ACTOR(GOAL_MARK);
    ACTOR(PLATFORM);
    ACTOR(PLATFORM_TURN);
    ACTOR(WATER);
    ACTOR(WATER_TRIGGER);
    ACTOR(BUSH);
    ACTOR(CLOUD);
    ACTOR(CLOUDS);
    ACTOR(COIN);
    ACTOR(WATER_SPLASH);
    ACTOR(BUBBLE);
}
