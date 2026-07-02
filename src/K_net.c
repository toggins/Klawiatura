#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_timer.h>

#include <NutBlast.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_discord.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_net.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_video.h"

#define MAX_GAME_PACKETS 32

static const char* last_error = NULL;

static ConnectState connect_state = CONN_NONE;

static LobbyInfo* lobby_list = NULL;
static size_t lobby_list_count = 0;

static NetID player_peers[MAX_PLAYERS] = {0}, spectator_peers[MAX_PEERS] = {0};

static void set_last_error(const char* error) {
    SDL_free((void*)last_error);
    last_error = (error == NULL || error[0] == '\0') ? NULL : SDL_strdup(error);
}

static void on_disconnected(const char* reason) {
    set_last_error(reason);
}

static void on_peer_joined(NetID pid) {
    chat_message(LFMT("chat_joined", 's', get_peer_name(pid)), B_YELLOW);
    play_generic_sound("ui/join", PLAY_SYSTEM);
}

static void on_peer_left(NetID pid) {
    if (!nuke_spectator_peer(pid)) {
        for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
            if (player_peers[i] != pid)
                continue;

            if (get_screen() != SCR_MENU)
                boot_to_menu(LFMT("msg_player_left", 's', get_peer_name(pid)));

            player_peers[i] = 0;
            break;
        }
    }

    chat_message(LFMT("chat_left", 's', get_peer_name(pid)), B_YELLOW);
    play_generic_sound("ui/leave", PLAY_SYSTEM);
}

static void clear_lobby_list() {
    for (size_t i = 0; i < lobby_list_count; i++)
        SDL_free((void*)lobby_list[i].name);

    SDL_free(lobby_list);
    lobby_list = NULL;
    lobby_list_count = 0;
}

static void on_lobbies_found(const NutBlast_Lobby* lobbies, size_t count) {
    clear_lobby_list();

    lobby_list_count = count;
    lobby_list = SDL_calloc(lobby_list_count, sizeof(*lobby_list));
    if (lobby_list == NULL) {
        lobby_list_count = 0;
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const NutBlast_Lobby* ldata = &lobbies[i];
        LobbyInfo* lobby = &lobby_list[i];

        lobby->peers = ldata->players;
        lobby->capacity = ldata->capacity;
        lobby->id = ldata->id;

        for (size_t j = 0; j < ldata->field_count; j++) {
            const NutBlast_LobbyField* field = &ldata->metadata[j];
            if (SDL_strcmp(field->key, NUTBLAST_FIELD_LOBBY_NAME) == 0) {
                lobby->name = SDL_strdup(
                    (SDL_strnlen(field->value, 33) > 32) ? fmt("%.*s...", 32, field->value) : field->value);
                break;
            }
        }
    }
}

static void on_master_changed(NetID) {
    chat_message(LFMT("chat_hosting", 's', get_peer_name(get_master_peer())), B_YELLOW);
}

static void on_peer_data_changed(NetID pid, NutBlast_FieldDiff diff) {
    if (SDL_strcmp(diff.name, NUTBLAST_FIELD_PLAYER_NAME) == 0) {
        chat_message(LFMT("chat_changed_name", 's', diff.old_value, 's', diff.new_value), B_YELLOW);
        return;
    }

    if (SDL_strcmp(diff.name, "spectator") == 0) {
        chat_message(
            LFMT((SDL_atoi(diff.new_value) > 0) ? "chat_spectator_on" : "chat_spectator_off", 's', get_peer_name(pid)),
            B_YELLOW);
        return;
    }
}

static void clear_peer_tables() {
    SDL_zeroa(player_peers);
    SDL_zeroa(spectator_peers);
}

void net_logger(NutBlast_LogLevel level, const char* message) {
    switch (level) {
    default:
        INFO("%s", message);
        break;
    case NB_LogError:
        WTF("%s", message);
        break;
    }
}

void net_init() {
    NutBlast_SetLogger(net_logger);
    NutBlast_SetGameID(fmt(GAME_NAME " " GAME_VERSION " %X", get_game_hash()));
    NutBlast_SetMaxChannels(PCH_SIZE);

    NutBlast_OnDisconnected(on_disconnected);
    NutBlast_OnPlayerJoined(on_peer_joined);
    NutBlast_OnPlayerLeft(on_peer_left);
    NutBlast_OnLobbiesFound(on_lobbies_found);
    NutBlast_OnMasterChanged(on_master_changed);
    NutBlast_OnPlayerMetadataChanged(on_peer_data_changed);

    clear_peer_tables();

    load_sound("ui/join", TRUE);
    load_sound("ui/leave", TRUE);
}

static void connect_state_success() {
    connect_state = CONN_SUCCESS;
    clear_chat();
    chat_message(LFMT("chat_connected"), B_YELLOW);
    update_discord_status();
}

void net_update() {
    NutBlast_Update();

    switch (connect_state) {
    default:
        break;

    case CONN_CONNECTING: {
        const int state = (last_error == NULL) ? is_connected() : -1;
        if (state > 0) {
            connect_state_success();
            INFO("Connect state ended successfully");
        } else if (state < 0) {
            connect_state = CONN_FAIL;
            WARN("Connect state ended in failure");
        }

        break;
    }
    }

    NutBlast_Message msg = {0};
    while (NutBlast_NextMessage(PCH_LOBBY, &msg)) {
        size_t size = msg.size;
        if (size < sizeof(PacketType))
            continue;

        const Uint8* data = (Uint8*)msg.data;
        const NetID from = msg.from;

        const PacketType ptype = *(PacketType*)data;
        if (ptype >= PT_MASTER_ONLY && (get_master_peer() != from || is_host()))
            continue;

        data += sizeof(PacketType);
        size -= sizeof(PacketType);

        switch (ptype) {
        default:
            break;

        case PT_CHAT: {
            if (size > 0 && CLIENT.show_user_messages)
                chat_message(fmt("%s: %.*s", get_peer_name(from), size, data), B_WHITE);
            break;
        }

        case PT_BAIL: {
            if (nuke_spectator_peer(from))
                break;

            for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
                if (player_peers[i] != from)
                    continue;

                if (get_screen() != SCR_MENU)
                    boot_to_menu(LFMT("msg_player_bailed", 's', get_peer_name(player_peers[i])));

                player_peers[i] = 0;
                break;
            }

            break;
        }

        case PT_START: {
            INFO("Got start packet");
            break;
        }

        case PT_END: {
            INFO("Got end packet");
            break;
        }
        }
    }
}

void net_teardown() {
    clear_lobby_list();
    set_last_error(NULL);
    disconnect();

    NutBlast_Cleanup();
}

void net_flush() {
    NutBlast_Flush();
}

void set_hostname(const char* hn) {
    NutBlast_SetNutBlaster((hn == NULL || hn[0] == '\0') ? NUTBLAST_DEFAULT_SERVER : hn);
}

Bool is_connected() {
    return NutBlast_IsReady();
}

Bool is_host() {
    return !is_connected() || get_master_peer() == get_local_peer();
}

Bool is_client() {
    return !is_host();
}

ConnectState get_connect_state() {
    return connect_state;
}

const char* net_error() {
    return last_error;
}

const NetID* get_peers() {
    return NutBlast_GetPlayerIDs();
}

NetID get_local_peer() {
    return NutBlast_GetOurID();
}

NetID get_master_peer() {
    return NutBlast_GetMasterID();
}

const char* get_peer_name(NetID pid) {
    return get_peer_string(pid, NUTBLAST_FIELD_PLAYER_NAME);
}

int get_peer_ping(NetID pid) {
    return (get_local_peer() == pid) ? NutBlast_ServerPing() : NutBlast_PlayerPing(pid);
}

const char* get_peer_string(NetID pid, const char* key) {
    return NutBlast_GetPlayerField(pid, key);
}

Sint32 get_peer_number(NetID pid, const char* key) {
    const char* data = NutBlast_GetPlayerField(pid, key);
    return (data == NULL) ? 0 : SDL_atoi(data);
}

Bool peer_exists(NetID pid) {
    return NutBlast_IsPlayerAlive(pid);
}

Uint8 get_peer_count() {
    return NutBlast_GetPlayerCount();
}

Uint8 get_peer_limit() {
    return NutBlast_GetMaxPlayers();
}

NetID player_to_peer(PlayerID pid) {
    return (pid < 0 || pid >= SDL_arraysize(player_peers)) ? 0 : player_peers[pid];
}

NetID spectator_to_peer(PlayerID sid) {
    return (sid < 0 || sid >= SDL_arraysize(spectator_peers)) ? 0 : spectator_peers[sid];
}

Bool nuke_spectator_peer(NetID pid) {
    if (pid <= 0)
        return FALSE;

    for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++) {
        if (spectator_peers[i] == pid) {
            chat_message(LFMT("chat_stopped_spectating", 's', get_peer_name(pid)), B_YELLOW);
            spectator_peers[i] = 0;
            return TRUE;
        }
    }

    return FALSE;
}

Uint8 get_lobby_player_count() {
    Uint8 count = 0;
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++)
        if (get_peer_number(*ptr, "spectator") <= 0)
            ++count;

    return count;
}

Uint8 get_lobby_spectator_count() {
    Uint8 count = 0;
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++)
        if (get_peer_number(*ptr, "spectator") > 0)
            ++count;

    return count;
}

Uint8 get_game_player_count() {
    Uint8 count = 0;
    for (size_t i = 0; i < SDL_arraysize(player_peers); i++)
        if (player_peers[i] > 0)
            ++count;

    return count;
}

Uint8 get_game_spectator_count() {
    Uint8 count = 0;
    for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++)
        if (spectator_peers[i] > 0)
            ++count;

    return count;
}

Bool i_am_spectating() {
    if (!is_connected())
        return FALSE;

    for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++)
        if (get_local_peer() == spectator_peers[i])
            return TRUE;

    return FALSE;
}

void host_lobby() {
    set_last_error(NULL);

    NutBlast_SetLobbyField(NUTBLAST_FIELD_LOBBY_NAME,
        (CLIENT.name[0] == '\0')
            ? "Lobby"
            : fmt("%s'%s Lobby", CLIENT.name, (CLIENT.name[SDL_strlen(CLIENT.name) - 1] == 's') ? "" : "s"));
    update_lobby_data();
    update_peer_data();
    NutBlast_Host(0, CLIENT.lobby_limit, !CLIENT.private_lobby);

    connect_state = CONN_CONNECTING;
}

void join_lobby(NetID lid) {
    set_last_error(NULL);

    update_peer_data();
    NutBlast_SetPlayerField("spectator", "0"); // GROSS HACK: pre-set `spectator` to trigger diff the first time.
    NutBlast_Join(lid);

    connect_state = CONN_CONNECTING;
}

void disconnect() {
    NutBlast_Disconnect();
    if (last_error == NULL) {
        if (get_screen() != SCR_MENU)
            boot_to_menu(LFMT("msg_disconnected"));
    } else {
        WARN("Disconnected with error: %s", last_error);
        if (get_screen() != SCR_MENU)
            boot_to_menu(fmt("%s\n%s", LFMT("msg_disconnected"), last_error));
    }

    NutBlast_PurgeMetadata();
    clear_lobby_list();
    clear_peer_tables();
    set_hostname(CLIENT.server);
    update_discord_status();
}

NetID get_lobby_id() {
    return NutBlast_GetLobbyID();
}

const char* get_lobby_name() {
    return NutBlast_GetLobbyField(NUTBLAST_FIELD_LOBBY_NAME);
}

const char* get_lobby_string(const char* key) {
    return NutBlast_GetLobbyField(key);
}

Sint32 get_lobby_number(const char* key) {
    const char* data = NutBlast_GetLobbyField(key);
    return (data == NULL) ? 0 : SDL_atoi(data);
}

Bool in_private_lobby() {
    return !NutBlast_IsListed();
}

void toggle_spectator() {
    NutBlast_SetPlayerField("spectator", fmt("%u", !get_peer_number(get_local_peer(), "spectator")));
}

void kick_peer(NetID pid) {
    NutBlast_Kick(pid);
}

void find_lobbies() {
    set_last_error(NULL);
    clear_lobby_list();
    NutBlast_FindLobbies(20);
}

const LobbyInfo* get_lobby_list(size_t idx) {
    return (idx < 0 || idx >= lobby_list_count) ? NULL : &lobby_list[idx];
}

size_t get_lobby_list_count() {
    return lobby_list_count;
}

void update_lobby_data() {
    NutBlast_SetLobbyField("world", CLIENT.world[0] == '\0' ? NULL : CLIENT.world);
}

void update_peer_data() {
    NutBlast_SetPlayerField(NUTBLAST_FIELD_PLAYER_NAME, CLIENT.name);
    NutBlast_SetPlayerField("character", fmt("%u", CLIENT.character));
    NutBlast_SetPlayerField("powerup", fmt("%u", CLIENT.powerup));
}

void peers_to_players(Uint8** cur) {
    clear_peer_tables();

    Uint8 num_players = 0, num_spectators = 0;

    // Fill player-to-peer and spectator-to-peer tables
    if (is_connected()) {
        for (const NetID* ptr = get_peers(); *ptr > 0; ptr++) {
            if (num_players >= SDL_arraysize(player_peers) || get_peer_number(*ptr, "spectator") > 0) {
                if (num_spectators < SDL_arraysize(spectator_peers))
                    spectator_peers[num_spectators++] = *ptr;
            } else {
                if (num_players < SDL_arraysize(player_peers))
                    player_peers[num_players++] = *ptr;
            }
        }
    }

    // Append to buffer
    if (cur != NULL) {
        BUFFER_WRITE8(*cur, &num_players);
        for (Uint8 i = 0; i < num_players; i++)
            BUFFER_WRITE64(*cur, &player_peers[i]);

        BUFFER_WRITE8(*cur, &num_spectators);
        for (Uint8 i = 0; i < num_spectators; i++)
            BUFFER_WRITE64(*cur, &spectator_peers[i]);
    }
}

static void net_send(GekkoNetAddress* gn_addr, const char* data, int len) {
    NutBlast_SendTo(PCH_GAME, *(NetID*)gn_addr->data, data, len);
}

static GekkoNetResult** net_receive(int* pcount) {
    static GekkoNetResult* packets[MAX_GAME_PACKETS] = {0};
    *pcount = 0;

    NutBlast_Message msg = {0};
    while (*pcount < MAX_GAME_PACKETS && NutBlast_NextMessage(PCH_GAME, &msg)) {
        GekkoNetResult* res = SDL_malloc(sizeof(*res));

        res->addr.size = sizeof(NetID);
        res->addr.data = SDL_malloc(res->addr.size);
        *(NetID*)res->addr.data = msg.from;

        res->data_len = msg.size;
        res->data = SDL_malloc(msg.size);
        SDL_memcpy(res->data, msg.data, msg.size);

        packets[(*pcount)++] = res;
    }

    return packets;
}

PlayerID populate_game(GekkoSession* session, PlayerID num_players) {
    static GekkoNetAdapter adapter = {
        .send_data = net_send,
        .receive_data = net_receive,
        .free_data = SDL_free,
    };

    if (!is_connected() || get_replay_state() == RPS_PLAYING) {
        for (PlayerID i = 0; i < num_players; i++)
            gekko_add_actor(session, GekkoLocalPlayer, NULL);

        return NULL_PLAYER;
    }

    gekko_net_adapter_set(session, &adapter);

    if (i_am_spectating()) {
        GekkoNetAddress addr = {0};
        addr.size = sizeof(*player_peers);
        addr.data = &player_peers[0];
        gekko_add_actor(session, GekkoRemotePlayer, &addr);

        return MAX_PLAYERS;
    }

    // Fill players
    INFO("PLAYERS:");

    Uint8 local = MAX_PLAYERS, tv = MAX_PLAYERS;
    for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
        if (player_peers[i] <= 0)
            continue;

        if (get_local_peer() == player_peers[i]) {
            local = i;
            gekko_add_actor(session, GekkoLocalPlayer, NULL);
            gekko_set_local_delay(session, local, CLIENT.input_delay);
        } else {
            GekkoNetAddress addr = {0};
            addr.size = sizeof(*player_peers);
            addr.data = &player_peers[i];
            gekko_add_actor(session, GekkoRemotePlayer, &addr);
        }

        // First valid player will host for spectators
        if (tv >= MAX_PLAYERS)
            tv = i;

        INFO("- %s%s", get_peer_name(player_peers[i]), i == local ? " (You)" : "");
    }

    // Fill spectators
    if (tv == local && local < MAX_PLAYERS) {
        INFO("");
        INFO("SPECTATORS:");

        for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++) {
            if (spectator_peers[i] <= 0)
                continue;

            GekkoNetAddress addr = {0};
            addr.size = sizeof(*spectator_peers);
            addr.data = &spectator_peers[i];
            gekko_add_actor(session, GekkoSpectator, &addr);

            INFO("- %s", get_peer_name(spectator_peers[i]));
        }
    }

    return (PlayerID)local;
}

Uint8* net_buffer() {
    static Uint8 data[NET_BUFFER_SIZE] = {0};
    return data;
}

void send_packet(PacketChannel channel, NetID pid, const Uint8* data, int size) {
    NutBlast_SendTo(channel, pid, (char*)data, size);
}

void send_reliable_packet(PacketChannel channel, NetID pid, const Uint8* data, int size) {
    NutBlast_SendReliablyTo(channel, pid, (char*)data, size);
}

void spread_packet(PacketChannel channel, const Uint8* data, int size) {
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++)
        send_packet(channel, *ptr, data, size);
}

void spread_reliable_packet(PacketChannel channel, const Uint8* data, int size) {
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++)
        send_reliable_packet(channel, *ptr, data, size);
}
