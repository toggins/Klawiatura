#pragma once

#include <SDL3/SDL_stdinc.h>

#include <gekkonet.h>
#include <nutpunch.h>

void net_init(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);

// Interface
void net_newframe();
bool is_connected();
const char* net_error();

// Lobbies
const char* get_lobby_id();
void host_lobby(const char*), join_lobby(const char*);
int find_lobby();
void disconnect();
