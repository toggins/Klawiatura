#pragma once

#include <SDL3/SDL_stdinc.h>
#include <gekkonet.h>

#include "K_game.h"

GekkoNetAdapter* net_init(const char* ip, char* name, PlayerID* pcount, char* level, GameFlags* flags);
void net_teardown();
void net_update(GekkoSession* session);
bool net_wait();
PlayerID net_fill(GekkoSession*);
