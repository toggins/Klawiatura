#pragma once

#include <gekkonet.h>

#include "K_misc.h"
#include "K_worlds.h"

#define CLIENT_STRING_MAX 32

typedef Uint64 NetID;

typedef Uint8 PacketType;
enum {
    PT_INVALID,

    PT_CHAT,
    PT_BAIL,

    PT_LEADER_ONLY,
    PT_WORLD,
    PT_GAME,

    PT_MASTER_ONLY,
    PT_PLAYERS,
};

#include "K_game.h"

#define NET_BUFFER_SIZE 1024
#define MAX_PEERS 8

typedef Uint8 ConnectState;
enum {
    CONN_NONE,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_DISCONNECTED,
};

typedef Uint8 PacketChannel;
enum {
    PCH_LOBBY,
    PCH_GAME,
    PCH_SIZE,
};

typedef struct {
    Uint8 peers, capacity;
    NetID id;
    const char* name;
} LobbyInfo;

void net_init(), net_update(), net_teardown();
void net_flush();

void set_hostname(const char*);
Bool is_connected(), is_host(), is_client(), is_leader();
ConnectState get_connect_state();
const char* net_error();

const NetID* get_peers();
NetID get_local_peer(), get_master_peer(), get_leading_peer();
const char* get_peer_name(NetID);
int get_peer_ping(NetID);
const char* get_peer_string(NetID, const char*);
Sint64 get_peer_number(NetID, const char*);
Bool get_peer_bool(NetID, const char*);
Bool peer_exists(NetID);
Uint8 get_peer_count(), get_peer_limit();

void bail_from_game();
NetID player_to_peer(PlayerID), spectator_to_peer(PlayerID);
Bool nuke_spectator_peer(NetID);
Uint8 get_lobby_player_count(), get_lobby_spectator_count();
Uint8 get_game_player_count(), get_game_spectator_count();
Bool i_am_spectating();

void host_lobby(), join_lobby(NetID), disconnect();
NetID get_lobby_id();
const char* get_lobby_name();
const char* get_lobby_string(const char*);
Sint64 get_lobby_number(const char*);
Bool get_lobby_bool(const char*);
Bool in_private_lobby();
void toggle_spectator();
void kick_peer(NetID);

void find_lobbies();
const LobbyInfo* get_lobby_list(size_t);
size_t get_lobby_list_count();

void update_lobby_data(), update_peer_data();

void peers_to_players();
PlayerID populate_game(GekkoSession*, PlayerID);

Buffer net_buffer();
void send_packet(PacketChannel, NetID, const Uint8*, size_t),
    send_reliable_packet(PacketChannel, NetID, const Uint8*, size_t);
void spread_packet(PacketChannel, const Uint8*, size_t), spread_reliable_packet(PacketChannel, const Uint8*, size_t);
void spread_reliable_packet_to_players(PacketChannel, const Uint8*, size_t);
void spread_world_packet(const WorldContext*), spread_game_packet(const GameContext*);
