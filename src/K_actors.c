#include "K_game.h"

#define ACTOR(ident)                                                                                                   \
    extern const GameActorTable TAB_##ident;                                                                           \
    ACTORS[ACT_##ident] = &TAB_##ident;

void POPULATE_ACTORS_TABLE() {
    extern const GameActorTable* ACTORS[ACT_SIZE];

    static const GameActorTable TAB_NULL = {NULL};
    ACTORS[ACT_NULL] = &TAB_NULL;
}
