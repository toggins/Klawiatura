#pragma once

#include <stdbool.h>

#include <SDL3/SDL_stdinc.h>

#include "gekkonet.h"

GekkoNetAdapter* nutpunch_init(int, const char*, const char*);
void net_teardown();
int net_wait(int*);
void net_fill(GekkoSession*);
