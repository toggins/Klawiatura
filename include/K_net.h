#pragma once

#include <SDL3/SDL_stdinc.h>

#include <NutPunch.h>
#include <gekkonet.h>

#define MAX_PEERS NUTPUNCH_MAX_PLAYERS

void net_init(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);

// Interface
void net_newframe();
bool is_connected();
const char* net_error();
void disconnect();

bool is_host(), is_client();
void push_user_data();

// Lobbies
const char* get_lobby_id();
void host_lobby(const char*), join_lobby(const char*);
void push_lobby_data(), pull_lobby_data();
void make_lobby_active();

int find_lobby();
void list_lobbies();
int get_lobby_count();
const NutPunch_LobbyInfo* get_lobby(int);
bool in_public_lobby();

// Peers
int get_peer_count();
bool peer_exists(int);
int player_to_peer(int8_t);
const char* get_peer_name(int);
int8_t populate_game(GekkoSession*);
