#include "K_game.h"

#define ACTOR(ident)                                                                                                   \
    extern const ActorTable TAB_##ident;                                                                               \
    ACTORS[ACT_##ident] = &TAB_##ident;

void POPULATE_ACTORS_TABLE() {
    extern const ActorTable* ACTORS[ACT_SIZE];

    static const ActorTable TAB_NULL = {NULL};
    ACTORS[ACT_NULL] = &TAB_NULL;

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
    ACTOR(COIN_POP);
    ACTOR(WATER_SPLASH);
    ACTOR(BUBBLE);
    ACTOR(BLOCK);
    ACTOR(BLOCK_BUMP);
    ACTOR(BRICK_SHARD);
    ACTOR(SUPER_MUSHROOM);
    ACTOR(FIRE_FLOWER);
    ACTOR(STARMAN);
    ACTOR(1UP_MUSHROOM);
    ACTOR(FIREBALL_PROJECTILE);
    ACTOR(BEETROOT_PROJECTILE);
    ACTOR(EXPLODE);
    ACTOR(GOOMBA);
    ACTOR(DEAD);
    ACTOR(KOOPA);
    ACTOR(KOOPA_SHELL);
    ACTOR(PIRANHA_PLANT);
    ACTOR(LAMP_LIGHT);
    ACTOR(TUBE_BUBBLES);
    ACTOR(TUBE_BUBBLE);
}
