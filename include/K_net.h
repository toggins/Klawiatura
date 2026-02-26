#pragma once

#define NUTPUNCH_NOSTD
#include <SDL3/SDL_stdinc.h>

#include <NutPunch.h>
#include <gekkonet.h>

#define LOBBY_STRING_MAX (sizeof(NutPunch_LobbyId))

#include "K_game.h"
#include "K_math.h"

#define MAX_PEERS (NUTPUNCH_MAX_PLAYERS)

typedef Uint8 LobbyVisibility;
enum {
	VIS_INVALID = 0,
	VIS_PUBLIC = 1,
	VIS_UNLISTED = 2,
};

typedef Uint8 MessageChannel;
enum {
	MCH_LOBBY = 0,
	MCH_GAME = 1,
};

typedef Uint8 MessageType;
enum {
	MT_START,
	MT_CHAT,
};

void net_init(), net_teardown();

const char* get_hostname();
void set_hostname(const char*);
Bool in_public_server();

// Interface
void net_newframe();
const char *net_error(), *net_verb();
void disconnect();

void push_user_data();

// Lobbies
const char* get_lobby_id();
void host_lobby(const char*), join_lobby(const char*);
void push_lobby_data(), pull_lobby_data();
void make_lobby_active();

void list_lobbies();
int get_lobby_count();
const NutPunch_LobbyInfo* get_lobby(int);
Bool in_public_lobby();
Uint32 get_lobby_party();

// Peers
int player_to_peer(PlayerID);
const char* get_peer_name(int);
PlayerID populate_game(GekkoSession*);
