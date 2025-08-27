#pragma once

#include <SDL3/SDL_stdinc.h>
#include <gekkonet.h>

#include "K_game.h"

GekkoNetAdapter* net_init(const char*, const char*);
void net_update();
void net_teardown();
void net_wait(PlayerID*, char*, GameFlags*);
PlayerID net_fill(GekkoSession*);
