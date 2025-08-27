#pragma once

#include <SDL3/SDL_stdinc.h>
#include <gekkonet.h>

#include "K_game.h"

GekkoNetAdapter* net_init(PlayerID, const char*, const char*);
void net_update();
void net_teardown();
PlayerID net_wait(PlayerID*, char*, GameFlags*);
void net_fill(GekkoSession*);
