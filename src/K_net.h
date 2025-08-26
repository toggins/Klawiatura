#pragma once

#include <SDL3/SDL_stdinc.h>
#include <gekkonet.h>

#include "K_game.h"

GekkoNetAdapter* nutpunch_init(int, const char*, const char*);
void net_teardown();
int net_wait(int*, GameFlags*);
void net_fill(GekkoSession*);
