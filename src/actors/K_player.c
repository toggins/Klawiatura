#include "K_game.h"

// NOTE: just an example on how to structure all the other actors...
//
// if you really need to use these structs outside the .c file, you're gonna have to put em in a header file next to
// this one

struct PlayerActorValues {
    EXTENDS_VALUES;
    ActorValue player;
    ActorValue frame;
    ActorValue ground;
    ActorValue spring;
    ActorValue power;
    ActorValue flash;
    ActorValue star, star_combo;
    ActorValue fire;
    ActorValue warp, warp_state;
    ActorValue platform;
};

struct PlayerActorFlags {
    EXTENDS_FLAGS;
    ActorFlag duck : 1, jump : 1, swim : 1;
    ActorFlag ascend : 1, descend : 1;
    ActorFlag respawn : 1;
    ActorFlag stomp : 1;
    ActorFlag warp_out : 1;
    ActorFlag dead : 1;
};

static void tick(struct GameState* gs, const ActorID id, struct GameActor* actor) {
    ACTOR_FLAGS(Player, actor)->dead = true;
    BASE_FLAGS(actor)->flip_x = !BASE_FLAGS(actor)->flip_x;
}

// don't forget to include it inside K_game.c
const struct GameActorInfo K_PLAYER = {NULL, NULL, tick, NULL, NULL};
