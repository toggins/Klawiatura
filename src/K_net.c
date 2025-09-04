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

static const char* lobby_id = NULL;
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
    lobby_id = random_lobby_id();
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

    static enum NetMenus {
        NM_MAIN,
        NM_SINGLE,
        NM_MULTI,
        NM_HOST,
        NM_JOIN,
        NM_LOBBY,
        NM_SIZE,
    } menu = NM_MAIN;

    static enum NetMenus menu_from[NM_SIZE] = {NM_MAIN};

    static size_t num_options[NM_SIZE] = {
        [NM_MAIN] = 3, [NM_SINGLE] = 3, [NM_MULTI] = 2, [NM_HOST] = 4, [NM_JOIN] = 0, [NM_LOBBY] = 0,
    };

    static size_t option[NM_SIZE] = {0};

    load_sound("SWITCH");
    load_sound("SELECT");

    for (;;) {
        const char** lobbies = NULL;
        if (menu == NM_JOIN) {
            lobbies = NutPunch_LobbyList((int*)(&(num_options[NM_JOIN])));
            if (option[NM_JOIN] > num_options[NM_JOIN])
                num_options[NM_JOIN] = SDL_max(0, num_options[NM_JOIN] - 1);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;

                case SDL_EVENT_QUIT:
                    goto quit;

                case SDL_EVENT_KEY_DOWN: {
                    switch (event.key.scancode) {
                        default:
                            break;

                        case SDL_SCANCODE_UP: {
                            if (num_options[menu] <= 0)
                                break;

                            if (option[menu] <= 0)
                                option[menu] = num_options[menu] - 1;
                            else
                                --(option[menu]);

                            play_ui_sound("SWITCH");
                            break;
                        }

                        case SDL_SCANCODE_DOWN: {
                            if (num_options[menu] <= 0)
                                break;

                            option[menu] = (option[menu] + 1) % num_options[menu];
                            play_ui_sound("SWITCH");
                            break;
                        }

                        case SDL_SCANCODE_Z: {
                            switch (menu) {
                                default:
                                    break;

                                case NM_MAIN: {
                                    switch (option[menu]) {
                                        default:
                                            break;

                                        case 0: {
                                            menu_from[NM_SINGLE] = menu;
                                            menu = NM_SINGLE;
                                            break;
                                        }

                                        case 1: {
                                            menu_from[NM_MULTI] = menu;
                                            menu = NM_MULTI;
                                            break;
                                        }

                                        case 2:
                                            goto quit;
                                    }

                                    play_ui_sound("SELECT");
                                    break;
                                }

                                case NM_SINGLE: {
                                    switch (option[menu]) {
                                        default:
                                            break;

                                        case 1: {
                                            if (*start_flags & GF_KEVIN)
                                                *start_flags &= ~GF_KEVIN;
                                            else
                                                *start_flags |= GF_KEVIN;

                                            play_ui_sound("SELECT");
                                            break;
                                        }

                                        case 2: {
                                            *num_players = 1;
                                            play_ui_sound("SELECT");
                                            return;
                                        }
                                    }

                                    break;
                                }

                                case NM_MULTI: {
                                    switch (option[menu]) {
                                        default:
                                            break;

                                        case 0: {
                                            if (*num_players <= 1)
                                                *num_players = 2;

                                            menu_from[NM_HOST] = menu;
                                            menu = NM_HOST;
                                            break;
                                        }

                                        case 1: {
                                            menu_from[NM_JOIN] = menu;
                                            menu = NM_JOIN;
                                            refresh_lobby_list();
                                            break;
                                        }
                                    }

                                    play_ui_sound("SELECT");
                                    break;
                                }

                                case NM_HOST: {
                                    switch (option[menu]) {
                                        default:
                                            break;

                                        case 0: {
                                            ++(*num_players);
                                            if (*num_players > MAX_PLAYERS)
                                                *num_players = 2;
                                            break;
                                        }

                                        case 2: {
                                            if (*start_flags & GF_KEVIN)
                                                *start_flags &= ~GF_KEVIN;
                                            else
                                                *start_flags |= GF_KEVIN;
                                            break;
                                        }

                                        case 3: {
                                            host_lobby(*num_players, level, *start_flags);
                                            menu_from[NM_LOBBY] = menu;
                                            menu = NM_LOBBY;
                                            break;
                                        }
                                    }

                                    play_ui_sound("SELECT");
                                    break;
                                }

                                case NM_JOIN: {
                                    if (lobbies == NULL || num_options[menu] <= 0)
                                        break;

                                    lobby_id = lobbies[option[menu]];
                                    NutPunch_SetServerAddr(server_ip);
                                    NutPunch_Join(lobby_id);
                                    menu_from[NM_LOBBY] = menu;
                                    menu = NM_LOBBY;

                                    play_ui_sound("SELECT");
                                    break;
                                }
                            }

                            break;
                        }

                        case SDL_SCANCODE_ESCAPE: {
                            if (menu == NM_LOBBY)
                                NutPunch_Disconnect();

                            if (menu_from[menu] != menu) {
                                menu = menu_from[menu];
                                play_ui_sound("SELECT");
                            }

                            break;
                        }
                    }
                    break;
                }
            }
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

        static char fmt[32];
        switch (menu) {
            default:
                break;

            case NM_MAIN: {
                draw_text(FNT_MAIN, FA_LEFT, "Singleplayer", (float[3]){40, 16, 0});
                draw_text(FNT_MAIN, FA_LEFT, "Multiplayer", (float[3]){40, 41, 0});
                draw_text(FNT_MAIN, FA_LEFT, "Exit", (float[3]){40, 66, 0});
                draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
                break;
            }

            case NM_SINGLE: {
                SDL_snprintf(fmt, sizeof(fmt), "Level: %s", level);
                draw_text(FNT_MAIN, FA_LEFT, fmt, (float[3]){40, 16, 0});

                SDL_snprintf(fmt, sizeof(fmt), "Kevin: %s", (*start_flags & GF_KEVIN) ? "ON" : "OFF");
                draw_text(FNT_MAIN, FA_LEFT, fmt, (float[3]){40, 41, 0});

                draw_text(FNT_MAIN, FA_LEFT, "Play!", (float[3]){40, 66, 0});
                draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
                break;
            }

            case NM_MULTI: {
                draw_text(FNT_MAIN, FA_LEFT, "Host Lobby", (float[3]){40, 16, 0});
                draw_text(FNT_MAIN, FA_LEFT, "Find Lobby", (float[3]){40, 41, 0});
                draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
                break;
            }

            case NM_HOST: {
                SDL_snprintf(fmt, sizeof(fmt), "Players: %i", *num_players);
                draw_text(FNT_MAIN, FA_LEFT, fmt, (float[3]){40, 16, 0});

                draw_text(FNT_MAIN, FA_LEFT, "Level: TEST", (float[3]){40, 41, 0});

                draw_text(
                    FNT_MAIN, FA_LEFT, (*start_flags & GF_KEVIN) ? "Kevin: ON" : "Kevin: OFF", (float[3]){40, 66, 0}
                );

                draw_text(FNT_MAIN, FA_LEFT, "Host!", (float[3]){40, 91, 0});
                draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
                break;
            }

            case NM_JOIN: {
                if (lobbies != NULL && num_options[NM_JOIN] > 0) {
                    for (size_t i = 0; i < num_options[NM_JOIN]; i++)
                        draw_text(FNT_MAIN, FA_LEFT, lobbies[i], (float[3]){40, 16 + ((float)i * 25), 0});
                    draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
                } else {
                    draw_text(FNT_MAIN, FA_LEFT, "No lobbies found", (float[3]){16, 16, 0});
                }
                break;
            }

            case NM_LOBBY: {
                if (lobby_id != NULL)
                    draw_text(FNT_MAIN, FA_LEFT, lobby_id, (float[3]){16, 16, 0});

                draw_text(FNT_MAIN, FA_LEFT, "Waiting for players", (float[3]){16, 41, 0});
                break;
            }
        }

        video_update(NULL);
        audio_update();

        SDL_Delay(1000 / TICKRATE);
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
