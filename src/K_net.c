#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free
#include <nutpunch.h>

#ifndef NUTPUNCH_WINDOSE
#error SORRY!!! NutPunch currently relies on Winsock
#endif

#include "K_game.h"
#include "K_log.h"
#include "K_net.h"

static const char *server_ip = NULL, *lobby_id = NULL;

static void send_data(GekkoNetAddress* gn_addr, const char* data, int len) {
    int peer = *(int*)gn_addr->data;
    if (!NutPunch_PeerAlive(peer))
        return;
    NutPunch_Send(peer, data, len);
}

static GekkoNetResult** receive_data(int* pCount) {
    static GekkoNetResult* packets[64] = {0};
    static char data[NUTPUNCH_BUFFER_SIZE] = {0};
    *pCount = 0;

    while (NutPunch_HasNext()) {
        int size = sizeof(data), peer = NutPunch_NextPacket(data, &size);
        GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));

        res->addr.size = sizeof(peer);
        res->addr.data = SDL_malloc(res->addr.size);
        SDL_memcpy(res->addr.data, &peer, res->addr.size);

        res->data_len = size;
        res->data = SDL_malloc(size);
        SDL_memcpy(res->data, data, size);

        packets[(*pCount)++] = res;
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
    if (*_num_players < 2)
        return;

    int size;
    NutPunch_SetServerAddr(server_ip);
    NutPunch_Join(lobby_id);
    NutPunch_Set("PLAYERS", sizeof(PlayerID), _num_players);
    NutPunch_Set("LEVEL", (int)SDL_strnlen(_level, 8), _level);
    NutPunch_Set("FLAGS", sizeof(GameFlags), _start_flags);
    INFO("Waiting in lobby \"%s\"... (FLAG: %d)", lobby_id, *_start_flags);

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) {
                net_teardown();
                exit(EXIT_SUCCESS);
            }

        switch (NutPunch_Update()) {
            case NP_Status_Error:
                FATAL("NutPunch_Query fail: %s", NutPunch_GetLastError());
                *_num_players = 0;
                return;

            case NP_Status_Online: {
                PlayerID* pplayers = NutPunch_Get("PLAYERS", &size);
                if (size == sizeof(PlayerID) && *pplayers)
                    *_num_players = *pplayers;

                char* plevel = NutPunch_Get("LEVEL", &size);
                if (size && size <= NUTPUNCH_FIELD_DATA_MAX)
                    SDL_memcpy(_level, plevel, size);

                GameFlags* pflags = NutPunch_Get("FLAGS", &size);
                if (size == sizeof(GameFlags))
                    *_start_flags = *pflags;

                if (*_num_players && NutPunch_PeerCount() + 1 >= *_num_players) {
                    INFO("%d player start!\n", *_num_players);
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
    int count = NutPunch_PeerCount();
    if (count > MAX_PLAYERS)
        count = MAX_PLAYERS;
    if (!count) {
        gekko_add_actor(session, LocalPlayer, NULL);
        return 0;
    }

    PlayerID counter = 0;
    static int indices[MAX_PLAYERS] = {0};
    static GekkoNetAddress addrs[MAX_PLAYERS] = {0};

    gekko_add_actor(session, LocalPlayer, NULL);
    for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) {
        if (!NutPunch_PeerAlive(i))
            continue;

        indices[counter] = i;
        addrs[counter].data = &indices[counter];
        addrs[counter].size = sizeof(*indices);
        gekko_add_actor(session, RemotePlayer, &addrs[counter]);

        if (++counter == count)
            break;
    }

    return 0;
}

void net_update() {
    NutPunch_Update();
}

void net_teardown() {
    NutPunch_Cleanup();
}
