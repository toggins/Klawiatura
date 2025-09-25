#pragma once

#include <SDL3/SDL_stdinc.h>

#include <gekkonet.h>
#include <nutpunch.h>

void net_init(), net_update(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);

void host_lobby(const char*), join_lobby(const char*);
