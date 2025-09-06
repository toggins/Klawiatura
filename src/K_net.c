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
#include "K_file.h"

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
static PlayerID* num_players = NULL; // moved this into a global for use below...
static GameFlags* start_flags = NULL;

static bool typing_level = false;
static char* level = NULL;
static char cache_level[NUTPUNCH_FIELD_DATA_MAX + 1] = {'\0'};

GekkoNetAdapter* net_init(const char* ip, PlayerID* pcount, char* init_level, GameFlags* flags) {
    SDL_memcpy(server_ip, ip, SDL_strnlen(ip, sizeof(server_ip)));
    level = init_level;
    SDL_strlcpy(cache_level, init_level, sizeof(cache_level));
    num_players = pcount;
    start_flags = flags;

    static GekkoNetAdapter adapter = {0};
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = SDL_free;
    return &adapter;
}

static const char *lobby_id = NULL, *MAGIC_KEY = "KLAWIATURA";
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

static enum NetMenu {
    NM_MAIN,
    NM_SINGLE,
    NM_MULTI,
    NM_HOST,
    NM_JOIN,
    NM_LOBBY,
    NM_SIZE,
} menu = NM_MAIN;

#define MENU_MAX_OPTIONS (16)
#define MENU_DISPLAY_SIZE (SDL_max(64, NUTPUNCH_ID_MAX + 1))

static enum NetMenu menu_from[NM_SIZE] = {NM_MAIN};
static int option[NM_SIZE] = {0};
static int menu_running = 1;

static void set_menu(enum NetMenu value) {
    menu_from[value] = menu;
    menu = value;
}

struct MenuOption {
    char display[MENU_DISPLAY_SIZE];
    void (*handle)();
};

static void noop() {}

static void go_single() {
    set_menu(NM_SINGLE);
}

static void go_multi() {
    set_menu(NM_MULTI);
}

static void go_host() {
    *num_players = 2;
    set_menu(NM_HOST);
}

static void go_join() {
    set_menu(NM_JOIN);
    refresh_lobby_list();
}

static void fucking_exit() {
    menu_running = -1;
}

static void toggle_pcount() {
    ++(*num_players);
    if (*num_players > MAX_PLAYERS)
        *num_players = 2;
}

static void type_level() {
    typing_level = true;
}

static void toggle_kevin() {
    if (*start_flags & GF_KEVIN)
        *start_flags &= ~GF_KEVIN;
    else
        *start_flags |= GF_KEVIN;

    stop_all_tracks();
    play_ui_track((*start_flags & GF_KEVIN) ? "PREDATOR" : "TITLE", true);
}

static void play_single() {
    if (find_file(file_pattern("data/levels/%s.kla", level), NULL) == NULL) {
        play_sound("BUMP");
        return;
    }

    play_sound("ENTER");
    *num_players = 1;
    menu_running = 0;
}

static void host_multi() {
    if (find_file(file_pattern("data/levels/%s.kla", level), NULL) == NULL) {
        play_sound("BUMP");
        return;
    }

    set_menu(NM_LOBBY);

    lobby_id = random_lobby_id();
    NutPunch_SetServerAddr(server_ip);
    if (NutPunch_Join(lobby_id))
        INFO("Waiting in lobby \"%s\"... (FLAG: %d)", lobby_id, *start_flags);

    NutPunch_LobbySet(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
    NutPunch_LobbySet("PLAYERS", sizeof(PlayerID), num_players);
    NutPunch_LobbySet("LEVEL", (int)SDL_strnlen(level, NUTPUNCH_FIELD_DATA_MAX - 1), level);
    NutPunch_LobbySet("FLAGS", sizeof(GameFlags), start_flags);
}

static void fucking_join() {
    if (lobby_id != NULL)
        return;

    int len = 0;
    lobby_id = NutPunch_LobbyList(&len)[option[menu]];
    NutPunch_SetServerAddr(server_ip);
    NutPunch_Join(lobby_id);

    set_menu(NM_LOBBY);
}

static struct MenuOption MENUS[NM_SIZE][MENU_MAX_OPTIONS] = {
    [NM_MAIN] =
        {
            {"Singleplayer", go_single},
            {"Multiplayer", go_multi},
            {"Exit", fucking_exit},
        },
    [NM_SINGLE] =
        {
            {"Level: FMT", type_level},
            {"Kevin: FMT", toggle_kevin},
            {"Play!", play_single},
        },
    [NM_MULTI] =
        {
            {"Host lobby", go_host},
            {"Find lobby", go_join},
        },
    [NM_HOST] =
        {
            {"Players: FMT", toggle_pcount},
            {"Level: FMT", type_level},
            {"Kevin: FMT", toggle_kevin},
            {"Host!", host_multi},
        },
    [NM_LOBBY] =
        {
            {"Lobby: FMT", noop},
            {"Waiting for players (FMT)", noop},
        },
    [NM_JOIN] = {},
};

static int num_options() {
    for (int i = 0; i < MENU_MAX_OPTIONS; i++)
        if (MENUS[menu][i].handle == NULL)
            return i;
    return MENU_MAX_OPTIONS;
}

static void handle_menu_input(SDL_Scancode key) {
    switch (key) {
        case SDL_SCANCODE_UP: {
            if (typing_level || menu == NM_LOBBY || num_options() <= 1)
                break;

            if (option[menu] <= 0)
                option[menu] = num_options() - 1;
            else
                --option[menu];

            play_ui_sound("SWITCH");
            break;
        }

        case SDL_SCANCODE_DOWN: {
            if (typing_level || menu == NM_LOBBY || num_options() <= 1)
                break;
            option[menu] = (option[menu] + 1) % num_options();
            play_ui_sound("SWITCH");
            break;
        }

        case SDL_SCANCODE_Z: {
            if (typing_level)
                break;

            struct MenuOption* opt = &MENUS[menu][option[menu]];
            if (opt->handle != noop) {
                play_ui_sound("SELECT");
                opt->handle();
            }
            break;
        }

        case SDL_SCANCODE_ESCAPE: {
            if (typing_level) {
                SDL_strlcpy(level, cache_level, sizeof(cache_level));
                typing_level = false;
                play_ui_sound("SELECT");
                break;
            }

            if (menu == NM_LOBBY) {
                NutPunch_Disconnect();
                lobby_id = NULL;
            }

            if (menu_from[menu] != menu) {
                menu = menu_from[menu];
                play_ui_sound("SELECT");
            }
            break;
        }

        case SDL_SCANCODE_RETURN: {
            if (typing_level) {
                SDL_strlcpy(cache_level, level, sizeof(cache_level));
                typing_level = false;
                play_ui_sound("SELECT");
            }
            break;
        }

        case SDL_SCANCODE_BACKSPACE: {
            if (typing_level && SDL_strlen(level) > 0)
                level[SDL_strlen(level) - 1] = '\0';
            break;
        }

        default: {
            if (typing_level && SDL_strlen(level) < sizeof(cache_level) - 1) {
                const char* gross = SDL_GetKeyName(SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false));
                if (SDL_strlen(gross) == 1)
                    SDL_strlcat(level, gross, sizeof(cache_level));
            }
            break;
        }
    }
}

static void display_lobbies() {
    int count = 0, i = 0;
    const char** lobbies = NutPunch_LobbyList(&count);
    if (count > MENU_MAX_OPTIONS)
        count = MENU_MAX_OPTIONS;
    for (; i < count; i++) {
        struct MenuOption* option = MENUS[NM_JOIN] + i;
        SDL_strlcpy(option->display, lobbies[i], MENU_DISPLAY_SIZE);
        option->handle = fucking_join;
    }
    if (!count) {
        struct MenuOption* option = MENUS[NM_JOIN] + i;
        SDL_strlcpy(option->display, "No lobbies found", MENU_DISPLAY_SIZE);
        option->handle = noop;
        i = 1;
    }
    for (; i < MENU_MAX_OPTIONS; i++)
        SDL_memset(&MENUS[NM_JOIN][i], 0, MENU_DISPLAY_SIZE);

    static int ticks_since_refresh = 0;
    if (menu == NM_JOIN && ticks_since_refresh++ >= 5 * TICKRATE) {
        refresh_lobby_list();
        ticks_since_refresh = 0;
    }
}

static bool is_ip_address(const char* str) {
    int count = 0;

    char* sep = SDL_strchr(str, '.');
    while (sep != NULL) {
        ++count;
        sep = SDL_strchr(sep + 1, '.');
    }

    if (count < 3) { // Might be IPv6 then
        count = 0;

        char* sep = SDL_strchr(str, ':');
        while (sep != NULL) {
            ++count;
            sep = SDL_strchr(sep + 1, ':');
        }
    }

    return count >= 3;
}

bool net_wait() {
    *num_players = 0;

    load_texture("Q_MAIN");
    load_sound("SWITCH");
    load_sound("SELECT");
    load_sound("BUMP");
    load_sound("ENTER");
    load_track("TITLE");
    load_track("PREDATOR");
    play_ui_track((*start_flags & GF_KEVIN) ? "PREDATOR" : "TITLE", true);

    menu_running = 1;
    while (menu_running > 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    return false;
                case SDL_EVENT_KEY_DOWN:
                    handle_menu_input(event.key.scancode);
                    break;
                default:
                    break;
            }
        }

        if (lobby_id != NULL)
            SDL_snprintf(MENUS[NM_LOBBY][0].display, MENU_DISPLAY_SIZE, "Lobby: %s", lobby_id);

        bool starting = false;
        switch (NutPunch_Update()) {
            case NP_Status_Error:
                FATAL("NutPunch_Query fail: %s", NutPunch_GetLastError());
                *num_players = 0;
                return false;

            case NP_Status_Online: {
                int size;

                PlayerID* pplayers = NutPunch_LobbyGet("PLAYERS", &size);
                if (size == sizeof(PlayerID) && *pplayers)
                    *num_players = *pplayers;

                char* plevel = NutPunch_LobbyGet("LEVEL", &size);
                if (size && size <= NUTPUNCH_FIELD_DATA_MAX)
                    SDL_memcpy(level, plevel, size);

                GameFlags* pflags = NutPunch_LobbyGet("FLAGS", &size);
                if (size == sizeof(GameFlags))
                    *start_flags = *pflags;

                if (*num_players && NutPunch_PeerCount() >= *num_players) {
                    INFO("%d player start!\n", *num_players);
                    starting = true;
                }

                break;
            }

            default:
                break;
        }

        static char fmt[MENU_DISPLAY_SIZE] = {0};
        display_lobbies();

        SDL_snprintf(fmt, sizeof(fmt), "Players: %d", *num_players);
        SDL_memcpy(MENUS[NM_HOST][0].display, fmt, sizeof(fmt));

        SDL_snprintf(fmt, sizeof(fmt), "Level: %s%s", level, (typing_level && (SDL_GetTicks() % 800) < 500) ? "_" : "");
        SDL_memcpy(MENUS[NM_SINGLE][0].display, fmt, sizeof(fmt));
        SDL_memcpy(MENUS[NM_HOST][1].display, fmt, sizeof(fmt));

        SDL_snprintf(fmt, sizeof(fmt), "Kevin: %s", *start_flags & GF_KEVIN ? "ON" : "OFF");
        SDL_memcpy(MENUS[NM_SINGLE][1].display, fmt, sizeof(fmt));
        SDL_memcpy(MENUS[NM_HOST][2].display, fmt, sizeof(fmt));

        SDL_snprintf(
            fmt, sizeof(fmt), "%s (%i/%i)", starting ? "Ready!" : "Waiting for players", NutPunch_PeerCount(),
            *num_players
        );
        SDL_memcpy(MENUS[NM_LOBBY][1].display, fmt, sizeof(fmt));

        video_update_custom_start();
        draw_rectangle("Q_MAIN", (float[2][2]){{0, 0}, {SCREEN_WIDTH, SCREEN_HEIGHT}}, 1, WHITE);
        for (int i = 0; i < num_options(); i++)
            draw_text(FNT_MAIN, FA_LEFT, MENUS[menu][i].display, (float[3]){40, (float)(16 + 25 * i), 0});
        if (menu != NM_LOBBY && num_options() > 1)
            draw_text(FNT_MAIN, FA_LEFT, ">", (float[3]){16, 16 + ((float)(option[menu]) * 25), 0});
        if (menu == NM_JOIN && !is_ip_address(server_ip)) {
            SDL_snprintf(fmt, sizeof(fmt), "Server: %s", server_ip);
            draw_text(FNT_MAIN, FA_LEFT, fmt, (float[3]){0, SCREEN_HEIGHT - string_height(FNT_MAIN, fmt), 0});
        }
        video_update_custom_end();

        audio_update();

        if (starting) {
            stop_all_tracks();
            set_menu(NM_MULTI);
            return true;
        }
        SDL_Delay(1000 / TICKRATE);
    }

    stop_all_tracks();
    return menu_running >= 0;
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
