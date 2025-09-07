#pragma once

#include <SDL3/SDL_stdinc.h>
#include <gekkonet.h>

#include "K_game.h"

#define CHAT_MSG_SIZE (64)
#define MAX_CHAT_MSGS (8)

GekkoNetAdapter* net_init(const char* ip, char* name, PlayerID* pcount, char* level, GameFlags* flags);
void net_teardown();
void net_update(GekkoSession* session);
bool net_wait();
PlayerID net_fill(GekkoSession*);
