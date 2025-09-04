#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free
#include <nutpunch.h>

#include "K_game.h"
#include "K_log.h"
#include "K_net.h"

static void send_data(GekkoNetAddress* gn_addr, const char* data, int len) {
    NutPunch_Send(*(int*)gn_addr->data, data, len);
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

static char server_ip[512] = {0};
GekkoNetAdapter* net_init(const char* _server_ip) {
    SDL_memcpy(server_ip, _server_ip, SDL_strnlen(_server_ip, sizeof(server_ip)));
    static GekkoNetAdapter adapter = {0};
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = SDL_free;
    return &adapter;
}

static const char* MAGIC_KEY = "KLAWIATURA";
static const uint8_t MAGIC_VALUE = 127;

static void refresh_lobby_list() {
    struct NutPunch_Filter filter = {0};
    SDL_memcpy(filter.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));
    SDL_memcpy(filter.value, &MAGIC_VALUE, sizeof(MAGIC_VALUE));
    filter.comparison = 0;

    NutPunch_SetServerAddr(server_ip);
    NutPunch_FindLobbies(1, &filter);
    INFO("Refreshing lobby list...");
}

static const char* random_lobby_id() {
    static uint8_t id[NUTPUNCH_ID_MAX + 1] = {0}, low = 'A', high = 'Z';
    for (int i = 0; i < 8; i++)
        id[i] = low + SDL_rand(high - low + 1);
    return (const char*)id;
}

static void host_lobby(PlayerID num_players, const char* level, GameFlags flags) {
    const char* lobby_id = random_lobby_id();
    NutPunch_SetServerAddr(server_ip);
    if (NutPunch_Join(lobby_id))
        INFO("Waiting in lobby \"%s\"... (FLAG: %d)", lobby_id, flags);
    NutPunch_Set(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
    NutPunch_Set("PLAYERS", sizeof(PlayerID), &num_players);
    NutPunch_Set("LEVEL", (int)SDL_strnlen(level, NUTPUNCH_FIELD_DATA_MAX - 1), level);
    NutPunch_Set("FLAGS", sizeof(GameFlags), &flags);
}

void net_wait(PlayerID* num_players, char* level, GameFlags* start_flags) {
    *num_players = 0;
    int selected_row = 0, ticks = 0;
    refresh_lobby_list();

    static enum NetMenus {
        NM_MAIN,
        NM_SINGLE,
        NM_MULTI,
        NM_LOBBIES,
        NM_LOBBY,
        NM_SIZE,
    } menu = NM_MAIN;
    static int option[NM_SIZE] = {0};

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT)
                goto quit;
        video_update(NULL);
        audio_update();

        const bool* kbd = SDL_GetKeyboardState(NULL);
        if (kbd[SDL_SCANCODE_ESCAPE])
            goto quit;

        if (kbd[SDL_SCANCODE_W])
            selected_row--;
        if (kbd[SDL_SCANCODE_S])
            selected_row++;

        int lobby_count = 0;
        const char** lobbies = NutPunch_LobbyList(&lobby_count);
        if (selected_row >= lobby_count)
            selected_row = lobby_count - 1;
        if (selected_row < 0)
            selected_row = 0;

        if (kbd[SDL_SCANCODE_R] || kbd[SDL_SCANCODE_F5])
            refresh_lobby_list();
        if ((kbd[SDL_SCANCODE_Z] || kbd[SDL_SCANCODE_RETURN]) && selected_row < lobby_count) {
            NutPunch_SetServerAddr(server_ip);
            NutPunch_Join(lobbies[selected_row]);
        }

        if (kbd[SDL_SCANCODE_1]) {
            *num_players = 1;
            return;
        }
        if (!*num_players && (kbd[SDL_SCANCODE_2] || kbd[SDL_SCANCODE_3] || kbd[SDL_SCANCODE_4])) {
            *num_players = (PlayerID)(2 * kbd[SDL_SCANCODE_2] + 3 * kbd[SDL_SCANCODE_3] + 4 * kbd[SDL_SCANCODE_4]);
            host_lobby(*num_players, level, *start_flags);
        }

        switch (NutPunch_Update()) {
            case NP_Status_Error:
                FATAL("NutPunch_Query fail: %s", NutPunch_GetLastError());
                *num_players = 0;
                return;

            case NP_Status_Online: {
                int size;

                PlayerID* pplayers = NutPunch_Get("PLAYERS", &size);
                if (size == sizeof(PlayerID) && *pplayers)
                    *num_players = *pplayers;

                char* plevel = NutPunch_Get("LEVEL", &size);
                if (size && size <= NUTPUNCH_FIELD_DATA_MAX)
                    SDL_memcpy(level, plevel, size);

                GameFlags* pflags = NutPunch_Get("FLAGS", &size);
                if (size == sizeof(GameFlags))
                    *start_flags = *pflags;

                if (*num_players && NutPunch_PeerCount() >= *num_players) {
                    INFO("%d player start!\n", *num_players);
                    return;
                }
                break;
            }

            default:
                break;
        }

        for (int i = 0; i < lobby_count; i++) {
            const float lh = 32.f;
            const float x = 12.f, y = 10.f + lh * (float)i;
            draw_rectangle(
                "", (float[2][2]){x, y, x + 300.f, y + lh}, 0.f,
                selected_row == i ? RGBA(255, 255, 255, 255) : RGBA(127, 127, 127, 255)
            );
            draw_text(FNT_MAIN, FA_LEFT, lobbies[i], (float[3]){x, y, 0.f});
        }

        SDL_Delay(1000 / TICKRATE);

        ticks += 1;
        if (!*num_players && (ticks * TICKRATE) >= 10000) {
            refresh_lobby_list();
            ticks = 0;
        }
    }

quit:
    net_teardown();
    exit(EXIT_SUCCESS);
}

PlayerID net_fill(GekkoSession* session) {
    int count = NutPunch_PeerCount();
    if (count > MAX_PLAYERS)
        count = MAX_PLAYERS;
    if (!count) {
        gekko_add_actor(session, LocalPlayer, NULL);
        return 0;
    }

    int counter = 0, local = MAX_PLAYERS;
    static int indices[MAX_PLAYERS] = {0};
    static GekkoNetAddress addrs[MAX_PLAYERS] = {0};

    for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) {
        if (!NutPunch_PeerAlive(i))
            continue;

        if (NutPunch_LocalPeer() == i) {
            local = counter;
            gekko_add_actor(session, LocalPlayer, NULL);
            INFO("You are player %d", local + 1);
        } else {
            indices[counter] = i;
            addrs[counter].data = indices + counter;
            addrs[counter].size = sizeof(*indices);
            gekko_add_actor(session, RemotePlayer, addrs + counter);
        }

        if (++counter == count)
            break;
    }

    return (PlayerID)local;
}

void net_update(GekkoSession* session) {
    NutPunch_Update();
    for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++)
        if (!NutPunch_PeerAlive(i)) {
            GekkoNetAddress addr = {0};
            addr.data = &i;
            addr.size = sizeof(i);
            gekko_remove_actor(session, addr);
        }
}

void net_teardown() {
    NutPunch_Cleanup();
}
