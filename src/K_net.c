#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <zlib.h>

#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#include <nutpunch.h>

#ifndef NUTPUNCH_WINDOSE
#error SORRY!!! NutPunch currently relies on Winsock
#endif

#include "K_game.h"
#include "K_log.h"
#include "K_net.h"

static const char* server_ip = NULL;
static const char* lobby_id = NULL;

static SOCKET sock = INVALID_SOCKET;
static GekkoNetAddress addrs[MAX_PLAYERS] = {0};

#define DATA_MAX (512000)
#define ZIP_DECOMPRESS (0)
#define ZIP_COMPRESS (1)

static const char* zippy(const char* input, int* len, uint8_t direction) {
    static char output[DATA_MAX] = {0};
    int ret = 0, destLen = sizeof(output);

    if (direction == ZIP_COMPRESS) {
        ret = compress2((Bytef*)output, (uLongf*)&destLen, (Bytef*)input, *len, Z_BEST_COMPRESSION);
        if (Z_OK != ret)
            goto skip;
    } else {
        ret = uncompress((Bytef*)output, (uLongf*)&destLen, (Bytef*)input, *len);
        if (Z_OK != ret)
            goto skip;
    }

    // printf("in: %d\tout: %d\n", *len, destLen);
    *len = destLen;
    return output;

skip:
    return input;
}

static void send_data(GekkoNetAddress* gn_addr, const char* data, int len) {
    if (!NutPunch_GetPeerCount() || gn_addr->data == NULL || !gn_addr->size)
        return;

    struct NutPunch* peer = (struct NutPunch*)(gn_addr->data);
    if (sock == INVALID_SOCKET) {
        // INFO("Socket died!!!");
        peer->port = 0;
        return;
    }
    if (!peer->port || 0xFFFFFFFF == *(uint32_t*)peer->addr)
        return;

    struct sockaddr_in ws_addr;
    ws_addr.sin_family = AF_INET;
    ws_addr.sin_port = htons(peer->port);
    SDL_memcpy(&ws_addr.sin_addr, peer->addr, 4);

    const char* defl = zippy(data, &len, ZIP_COMPRESS);
    if (defl == NULL) {
        INFO("Failed to deflate (%d)\n", len);
        return;
    }

    int io = sendto(sock, defl, len, 0, (struct sockaddr*)&ws_addr, sizeof(ws_addr));

    if (SOCKET_ERROR == io && WSAGetLastError() != WSAEWOULDBLOCK) {
        INFO("Failed to send to peer %d (%d)\n", peer->port, WSAGetLastError());
        peer->port = 0; // just nuke them...
    }
    if (!io)
        peer->port = 0; // graceful close
}

static GekkoNetResult* make_packet(int peer_idx, const char* data, int io) {
    GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));

    res->addr.size = sizeof(struct NutPunch);
    res->addr.data = SDL_malloc(res->addr.size);
    SDL_memcpy(res->addr.data, &NutPunch_GetPeers()[peer_idx], res->addr.size);

    const char* infl = zippy(data, &io, ZIP_DECOMPRESS);
    if (infl == NULL) {
        INFO("Failed to deflate (%d)\n", io);
        return NULL;
    }

    res->data_len = io;
    res->data = SDL_malloc(io);
    SDL_memcpy(res->data, infl, io);

    return res;
}

static GekkoNetResult** receive_data(int* length) {
    if (!NutPunch_GetPeerCount() || sock == INVALID_SOCKET)
        return NULL;

    static char data[DATA_MAX] = {0};
    static GekkoNetResult* packets[64] = {0};
    *length = 0;

    for (;;) { // accept connections
        static struct timeval instantBitchNoodles = {0, 0};
        fd_set s = {1, {sock}};

        int res = select(0, &s, NULL, NULL, &instantBitchNoodles);
        if (res == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            INFO("Failed to poll socket: %d\n", WSAGetLastError());
        }
        if (!res)
            break;

        struct sockaddr_in addr = {0};
        int addrSize = sizeof(addr);

        SDL_memset(data, 0, sizeof(data));
        int io = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr*)&addr, &addrSize);

        if (io == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            INFO("Failed to receive from socket (%d)\n", WSAGetLastError());
            closesocket(sock);
            sock = INVALID_SOCKET;
            break;
        }
        if (io <= 0)
            continue;

        for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) {
            struct NutPunch* peer = &NutPunch_GetPeers()[i];
            if (NutPunch_LocalPeer() == i || !peer->port)
                continue;

            bool same_host = !SDL_memcmp(&addr.sin_addr, peer->addr, 4);
            bool same_port = addr.sin_port == htons(peer->port);
            if (!same_host || !same_port)
                continue;

            packets[(*length)++] = make_packet(i, data, io);
            break;
        }
    }

    for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) { // process existing peers
        struct NutPunch* peer = &NutPunch_GetPeers()[i];
        if (NutPunch_LocalPeer() == i || !peer->port)
            continue;

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(peer->port);
        SDL_memcpy(&addr.sin_addr, peer->addr, 4);
        int addrSize = sizeof(addr);

        SDL_memset(data, 0, sizeof(data));
        int io = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr*)&addr, &addrSize);

        if (io == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            INFO("Failed to receive from peer %d (%d)\n", peer->port, WSAGetLastError());
            peer->port = 0; // just nuke them...
            continue;
        }
        if (!io) { // graceful close
            INFO("Peer %d disconnect\n", i + 1);
            peer->port = 0;
        }
        if (io <= 0)
            continue;
        packets[(*length)++] = make_packet(i, data, io);
    }

    return packets;
}

GekkoNetAdapter* net_init(const char* _server_ip, const char* _lobby_id) {
    server_ip = _server_ip;
    lobby_id = _lobby_id;

    static GekkoNetAdapter adapter = {0};
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = SDL_free;
    return &adapter;
}

void net_wait(PlayerID* _num_players, char* _level, GameFlags* _start_flags) {
    if (*_num_players <= 1)
        return;

    int size;
    NutPunch_SetServerAddr(server_ip);
    NutPunch_Join(lobby_id);
    NutPunch_Set("PLAYERS", sizeof(PlayerID), _num_players);
    NutPunch_Set("LEVEL", (int)SDL_strnlen(_level, 8), _level);
    NutPunch_Set("FLAGS", sizeof(GameFlags), _start_flags);
    INFO("Waiting in lobby \"%s\"... (FLAG: %d)", NutPunch_LobbyId, *_start_flags);

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) {
                net_teardown();
                exit(EXIT_SUCCESS);
            }

        switch (NutPunch_Query()) {
            case NP_Status_Error:
                FATAL("NutPunch_Query fail: %s", NutPunch_GetLastError());
                break;

            case NP_Status_Punched: {
                PlayerID* pplayers = NutPunch_Get("PLAYERS", &size);
                if (size == sizeof(PlayerID) && *pplayers)
                    *_num_players = *pplayers;

                char* plevel = NutPunch_Get("LEVEL", &size);
                if (size && size <= NUTPUNCH_FIELD_DATA_MAX)
                    SDL_memcpy(_level, plevel, size);

                GameFlags* pflags = NutPunch_Get("FLAGS", &size);
                if (size == sizeof(GameFlags))
                    *_start_flags = *pflags;

                if (*_num_players && NutPunch_GetPeerCount() >= *_num_players) {
                    INFO("%d player start!\n", *_num_players);
                    sock = NutPunch_Done();
                    return;
                }
                break;
            }
            default:
                break;
        }

        SDL_Delay(1000 / TICKRATE);
    }
}

PlayerID net_fill(GekkoSession* session) {
    int size;
    PlayerID* count = NutPunch_Get("PLAYERS", &size);
    if (size != sizeof(*count) || !*count) {
        gekko_add_actor(session, LocalPlayer, NULL);
        return 0;
    }

    PlayerID counter = 0, local = MAX_PLAYERS;
    SDL_memset(addrs, 0, sizeof(addrs));

    for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) {
        struct NutPunch* peer = &NutPunch_GetPeers()[i];
        if (!peer->port)
            continue;

        addrs[counter].data = peer;
        addrs[counter].size = sizeof(*peer);

        if (i == NutPunch_LocalPeer()) {
            gekko_add_actor(session, LocalPlayer, NULL);
            local = counter;
            INFO("You are peer %i", i);
        } else {
            gekko_add_actor(session, RemotePlayer, &addrs[counter]);
        }

        if (++counter == *count)
            break;
    }

    return local;
}

void net_update() {
    NutPunch_Query();
}

void net_teardown() {
    NutPunch_Cleanup();
}
