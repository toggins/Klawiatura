#include "K_game.h"

#define ACTOR(ident)                                                                                                   \
    extern const ActorTable TAB_##ident;                                                                               \
    ACTORS[ACT_##ident] = &TAB_##ident;

void POPULATE_ACTORS_TABLE() {
    extern const ActorTable* ACTORS[ACT_SIZE];

    ACTOR(PLAYER_SPAWN);
    ACTOR(PLAYER);
    ACTOR(WATER);
    ACTOR(WATER_TRIGGER);
    ACTOR(DUMMY);

    static const ActorTable TAB_NULL = {NULL};
    ACTORS[ACT_NULL] = &TAB_NULL;
}
