#include <stdlib.h>

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#define NUTPUNCH_IMPLEMENTATION
#include "nutpunch.h"

#ifndef NUTPUNCH_WINDOSE
#error SORRY!!! NutPunch currently relies on "winsock2.h"
#endif

#include "K_game.h"
#include "K_log.h"
#include "K_net.h"

static int num_players = 1;
static const char* server_ip = NULL;
static const char* lobby_id = NULL;
static SOCKET sock = INVALID_SOCKET;
static GekkoNetAddress addrs[MAX_PLAYERS] = {0};

static void send_data(GekkoNetAddress* addr, const char* data, int len) {
    if (num_players == 1)
        return;

    struct NutPunch* peer = (struct NutPunch*)addr->data;
    if (sock == INVALID_SOCKET) {
        // INFO("Socket died!!!");
        peer->port = 0;
        return;
    }
    if (!peer->port)
        return;

    struct sockaddr_in baseAddr;
    baseAddr.sin_family = AF_INET;
    baseAddr.sin_port = htons(peer->port);
    SDL_memcpy(&baseAddr.sin_addr, peer->addr, 4);
    struct sockaddr* sockAddr = (struct sockaddr*)&baseAddr;

    int io = sendto(sock, data, len, 0, sockAddr, sizeof(baseAddr));
    if (SOCKET_ERROR == io && WSAGetLastError() != WSAEWOULDBLOCK) {
        INFO("Failed to send to peer %d (%d)\n", peer->port, WSAGetLastError());
        peer->port = 0; // just nuke them...
    }
    if (!io)
        peer->port = 0; // graceful close
}

static GekkoNetResult* make_result(int peer_idx, const char* data, int io) {
    GekkoNetResult* res = SDL_malloc(sizeof(*res));

    res->addr.size = sizeof(struct NutPunch);
    res->addr.data = SDL_malloc(res->addr.size);
    SDL_memcpy(res->addr.data, &NutPunch_GetPeers()[peer_idx], res->addr.size);

    res->data_len = io;
    res->data = SDL_malloc(res->data_len);
    SDL_memcpy(res->data, data, res->data_len);

    return res;
}

static GekkoNetResult** receive_data(int* length) {
    if (num_players == 1)
        return NULL;
    if (sock == INVALID_SOCKET) {
        // INFO("Socket died!!! Catching on fire NOW!!");
        return NULL;
    }

    NutPunch_Query();               // just in case.....
    static char data[512000] = {0}; // larger than SaveState for leeway

    static GekkoNetResult* results[64] = {0};
    *length = 0;

    for (;;) {
        struct sockaddr_in base_addr = {0};
        int addrSize = sizeof(base_addr);

        SDL_memset(data, 0, sizeof(data));
        int io = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr*)&base_addr, &addrSize);

        if (io == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            INFO("Failed to receive from socket (%d)\n", WSAGetLastError());
            closesocket(sock);
            sock = INVALID_SOCKET;
            break;
        }
        if (io <= 0 || io > sizeof(data))
            continue;

        for (int i = 0; i < num_players; i++) {
            struct NutPunch* peer = &NutPunch_GetPeers()[i];
            if (NutPunch_LocalPeer() == i || !peer->port)
                continue;

            bool same_host = !SDL_memcmp(&base_addr.sin_addr, peer->addr, 4);
            bool same_port = base_addr.sin_port == htons(peer->port);
            if (!same_host || !same_port)
                continue;

            results[(*length)++] = make_result(i, data, io);
            break;
        }
    }

    for (int i = 0; i < num_players; i++) {
        struct NutPunch* peer = &NutPunch_GetPeers()[i];
        if (NutPunch_LocalPeer() == i || !peer->port)
            continue;

        struct sockaddr_in base_addr;
        base_addr.sin_family = AF_INET;
        base_addr.sin_port = htons(peer->port);
        SDL_memcpy(&base_addr.sin_addr, peer->addr, 4);
        int addrSize = sizeof(base_addr);

        SDL_memset(data, 0, sizeof(data));
        int io = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr*)&base_addr, &addrSize);

        if (io == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            INFO("Failed to receive from peer %d (%d)\n", i + 1, WSAGetLastError());
            NutPunch_GetPeers()[i].port = 0; // just nuke them...
            continue;
        }
        if (!io) { // graceful close
            INFO("Peer %d disconnect\n", i + 1);
            NutPunch_GetPeers()[i].port = 0;
        }
        if (io <= 0 || io > sizeof(data))
            continue;
        results[(*length)++] = make_result(i, data, io);
    }

    return results;
}

GekkoNetAdapter* nutpunch_init(int _num_players, const char* _server_ip, const char* _lobby_id) {
    num_players = _num_players;
    server_ip = _server_ip;
    lobby_id = _lobby_id;

    static GekkoNetAdapter adapter = {0};
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = SDL_free;
    return &adapter;
}

int net_wait(GekkoSession* session) {
    if (num_players <= 1) {
        gekko_add_actor(session, LocalPlayer, NULL);
        return 0;
    }

    NutPunch_SetServerAddr(server_ip);
    NutPunch_Join(lobby_id);
    INFO("About to punch your nuts... Lobby: '%s'", NutPunch_LobbyId);

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (SDL_EVENT_QUIT == event.type) {
                gekko_destroy(session);
                session = NULL;
                exit(EXIT_SUCCESS);
            }
        }

        if (NP_Status_Started == NutPunch_Query())
            break;
        if (NutPunch_GetPeerCount() >= num_players)
            NutPunch_Start();

        SDL_Delay(1000 / 60);
    }

    sock = NutPunch_Done();
    INFO("Nuts have been punched!");

    for (int i = 0; i < num_players; i++) {
        addrs[i].data = &NutPunch_GetPeers()[i];
        addrs[i].size = sizeof(struct NutPunch);
        if (NutPunch_LocalPeer() == i)
            gekko_add_actor(session, LocalPlayer, NULL);
        else
            gekko_add_actor(session, RemotePlayer, &addrs[i]);
    }

    return NutPunch_LocalPeer();
}

void net_teardown() {
    NutPunch_Cleanup();
}
