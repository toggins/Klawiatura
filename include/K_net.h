#pragma once

#include <SDL3/SDL_stdinc.h>

#include <NutPunch.h>
#include <gekkonet.h>

#define MAX_PEERS NUTPUNCH_MAX_PLAYERS

typedef uint8_t LobbyVisibility;
enum {
	VIS_INVALID = 0,
	VIS_PUBLIC = 1,
	VIS_UNLISTED = 2,
};

void net_init(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);
bool in_public_server();

// Interface
void net_newframe();
bool is_connected();
const char *net_error(), *net_verb();
void disconnect();

bool is_host(), is_client();
void push_user_data();

// Lobbies
const char* get_lobby_id();
void host_lobby(const char*), join_lobby(const char*);
void push_lobby_data(), pull_lobby_data();
void make_lobby_active();

void list_lobbies();
int get_lobby_count();
const NutPunch_LobbyInfo* get_lobby(int);
bool in_public_lobby();
uint32_t get_lobby_party();

// Peers
int get_peer_count(), get_max_peers();
bool peer_exists(int);
int player_to_peer(int8_t);
const char* get_peer_name(int);
int8_t populate_game(GekkoSession*);
