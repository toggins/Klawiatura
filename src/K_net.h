#pragma once

#include <stdbool.h>

#include <SDL3/SDL_stdinc.h>

#include "gekkonet.h"

GekkoNetAdapter* nutpunch_init(int);
void net_teardown();
int net_wait(GekkoSession*);
