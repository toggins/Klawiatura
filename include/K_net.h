#pragma once

#include <gekkonet.h>

#include "K_misc.h"

#define CLIENT_STRING_MAX 32

typedef Uint64 NetID;

typedef Uint8 PacketType;
enum {
    PT_CHAT,
    PT_BAIL,

    PT_MASTER_ONLY,
    PT_START,
    PT_END,
};

#include "K_game.h"

#define NET_BUFFER_SIZE 1024
#define MAX_PEERS 8

#define BUFFER_START(buf, cur) Uint8*(cur) = (buf);

#define BUFFER_READ8(cur, dest)                                                                                        \
    do {                                                                                                               \
        *(Uint8*)(dest) = *(Uint8*)(cur);                                                                              \
        (cur) += sizeof(Uint8);                                                                                        \
    } while (FALSE);

#define BUFFER_READ16(cur, dest)                                                                                       \
    do {                                                                                                               \
        *(Uint16*)(dest) = SDL_Swap16BE(*(Uint16*)(cur));                                                              \
        (cur) += sizeof(Uint16);                                                                                       \
    } while (FALSE);

#define BUFFER_READ32(cur, dest)                                                                                       \
    do {                                                                                                               \
        *(Uint32*)(dest) = SDL_Swap32BE(*(Uint32*)(cur));                                                              \
        (cur) += sizeof(Uint32);                                                                                       \
    } while (FALSE);

#define BUFFER_READ64(cur, dest)                                                                                       \
    do {                                                                                                               \
        *(Uint64*)(dest) = SDL_Swap64BE(*(Uint64*)(cur));                                                              \
        (cur) += sizeof(Uint64);                                                                                       \
    } while (FALSE);

#define BUFFER_READ_STRING(cur, dest, size)                                                                            \
    do {                                                                                                               \
        SDL_strlcpy((char*)(dest), (char*)(cur), size);                                                                \
        (cur) += (size);                                                                                               \
    } while (FALSE);

#define BUFFER_WRITE8(cur, src)                                                                                        \
    do {                                                                                                               \
        *(Uint8*)(cur) = *(Uint8*)(src);                                                                               \
        (cur) += sizeof(Uint8);                                                                                        \
    } while (FALSE);

#define BUFFER_WRITE16(cur, src)                                                                                       \
    do {                                                                                                               \
        *(Uint16*)(cur) = SDL_Swap16BE(*(Uint16*)(src));                                                               \
        (cur) += sizeof(Uint16);                                                                                       \
    } while (FALSE);

#define BUFFER_WRITE32(cur, src)                                                                                       \
    do {                                                                                                               \
        *(Uint32*)(cur) = SDL_Swap32BE(*(Uint32*)(src));                                                               \
        (cur) += sizeof(Uint32);                                                                                       \
    } while (FALSE);

#define BUFFER_WRITE64(cur, src)                                                                                       \
    do {                                                                                                               \
        *(Uint64*)(cur) = SDL_Swap64BE(*(Uint64*)(src));                                                               \
        (cur) += sizeof(Uint64);                                                                                       \
    } while (FALSE);

#define BUFFER_WRITE_STRING(cur, src, size)                                                                            \
    do {                                                                                                               \
        SDL_strlcpy((char*)(cur), (char*)(src), size);                                                                 \
        (cur) += (size);                                                                                               \
    } while (FALSE);

typedef Uint8 ConnectState;
enum {
    CONN_NONE,
    CONN_CONNECTING,
    CONN_SUCCESS,
    CONN_FAIL,
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
Bool is_connected(), is_host(), is_client();
ConnectState get_connect_state();
const char* net_error();

const NetID* get_peers();
NetID get_local_peer(), get_master_peer();
const char* get_peer_name(NetID);
int get_peer_ping(NetID);
const char* get_peer_string(NetID, const char*);
Sint32 get_peer_number(NetID, const char*);
Bool peer_exists(NetID);
Uint8 get_peer_count(), get_peer_limit();

NetID player_to_peer(PlayerID), spectator_to_peer(PlayerID);
Bool nuke_spectator_peer(NetID);
Uint8 get_lobby_player_count(), get_lobby_spectator_count();
Uint8 get_game_player_count(), get_game_spectator_count();
Bool i_am_spectating();

void host_lobby(), join_lobby(NetID), disconnect();
NetID get_lobby_id();
const char* get_lobby_string(const char*);
Sint32 get_lobby_number(const char*);
Bool in_private_lobby();
void toggle_spectator(), toggle_ready();

void find_lobbies();
const LobbyInfo* get_lobby_list(size_t);
size_t get_lobby_list_count();

void push_user_data();

void peers_to_players(Uint8**);
PlayerID populate_game(GekkoSession*, PlayerID);

Uint8* net_buffer();
void send_packet(PacketChannel, NetID, const Uint8*, int),
    send_reliable_packet(PacketChannel, NetID, const Uint8*, int);
void spread_packet(PacketChannel, const Uint8*, int), spread_reliable_packet(PacketChannel, const Uint8*, int);
