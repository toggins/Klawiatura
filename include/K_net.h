#pragma once

#include <SDL3/SDL_stdinc.h>

#include <gekkonet.h>
#include <nutpunch.h>

#define MAX_PEERS NUTPUNCH_MAX_PLAYERS

void net_init(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);

// Interface
void net_newframe();
bool is_connected();
const char* net_error();
void disconnect();

bool is_host();
bool is_client();

// Lobbies
const char* get_lobby_id();
void host_lobby(const char*), join_lobby(const char*);

int find_lobby();
void list_lobbies();
int get_lobby_count();
const char* get_lobby(int);

// Peers
int get_peer_count();
bool peer_exists(int);
const char* get_peer_name(int);
int8_t populate_game(GekkoSession*);
