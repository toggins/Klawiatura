#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_timer.h>

#include <NutBlast.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_discord.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_net.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_video.h"
#include "K_worlds.h"

#define MAX_GAME_PACKETS 32

static const char* last_error = NULL;

static ConnectState connect_state = CONN_NONE;

static LobbyInfo* lobby_list = NULL;
static size_t lobby_list_count = 0;

static NetID player_peers[MAX_PLAYERS] = {0}, spectator_peers[MAX_PEERS] = {0};

static void on_ready() {
    if (connect_state == CONN_CONNECTING)
        INFO("Connect state ended successfully");
    connect_state = CONN_CONNECTED;

    clear_chat();
    chat_message(LFMT("chat_connected"), B_YELLOW);
    update_discord_status();
}

static void set_last_error(const char* error) {
    SDL_free((void*)last_error);
    last_error = (error == NULL || error[0] == '\0') ? NULL : SDL_strdup(error);
}

static Bool from_on_disconnected = FALSE;
static void on_disconnected(NutBlast_Reason reason) {
    set_last_error((SDL_strcmp(reason.code, NUTBLAST_ERROR_OK) == 0) ? NULL : reason.code);
    from_on_disconnected = TRUE;
    disconnect();
    from_on_disconnected = FALSE;
}

static void on_peer_joined(NetID pid) {
    chat_message(LFMT("chat_joined", 's', get_peer_name(pid)), B_YELLOW);
    play_generic_sound("ui/join", PLAY_SYSTEM);
}

static void on_peer_left(NetID pid, NutBlast_Reason reason) {
    const Bool has_reason = SDL_strcmp(reason.code, NUTBLAST_ERROR_OK) != 0;

    if (!nuke_spectator_peer(pid)) {
        for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
            if (player_peers[i] != pid)
                continue;

            if (get_screen() != SCR_MENU) {
                const char* lstr = LFMT("msg_player_left", 's', get_peer_name(pid));
                boot_to_menu(has_reason ? fmt("%s\n%s", lstr, reason.code) : lstr);
            }

            player_peers[i] = 0;
            break;
        }
    }

    const char* lstr = LFMT("chat_left", 's', get_peer_name(pid));
    chat_message(has_reason ? fmt("%s (%s)", lstr, reason.code) : lstr, B_YELLOW);
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

static void on_master_changed(NetID pid) {
    if (pid > 0)
        chat_message(LFMT("chat_hosting", 's', get_peer_name(get_master_peer())), B_YELLOW);
}

static void on_peer_data_changed(NetID pid, NutBlast_FieldDiff diff) {
    if (diff.old_value == NULL)
        return;

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

static void clear_player_peer_tables() {
    SDL_zeroa(player_peers);
    SDL_zeroa(spectator_peers);
}

static void net_logger(NutBlast_LogLevel level, const char* message) {
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

    NutBlast_OnReady(on_ready);
    NutBlast_OnDisconnected(on_disconnected);
    NutBlast_OnPlayerJoined(on_peer_joined);
    NutBlast_OnPlayerLeft(on_peer_left);
    NutBlast_OnLobbiesFound(on_lobbies_found);
    NutBlast_OnMasterChanged(on_master_changed);
    NutBlast_OnPlayerMetadataChanged(on_peer_data_changed);

    clear_player_peer_tables();

    load_sound("ui/join", AKL_ALWAYS);
    load_sound("ui/leave", AKL_ALWAYS);
}

static Bool i_am_in_tables() {
    const NetID me = get_local_peer();

    for (size_t i = 0; i < SDL_arraysize(player_peers); i++)
        if (me == player_peers[i])
            return TRUE;

    return i_am_spectating();
}

void net_update() {
    NutBlast_Update();

    NutBlast_Message msg = {0};
    while (NutBlast_NextMessage(PCH_LOBBY, &msg)) {
        if (msg.size < sizeof(PacketType))
            continue;

        const PacketType ptype = *(PacketType*)msg.data;
        if ((ptype >= PT_LEADER_ONLY && ptype < PT_MASTER_ONLY)
            && (get_leading_peer() != msg.from && get_master_peer() != msg.from))
        {
            continue;
        }
        if (ptype >= PT_MASTER_ONLY && get_master_peer() != msg.from)
            continue;

        Buffer mbuf = buffer_from((void*)(msg.data + sizeof(PacketType)), msg.size - 1);
        switch (ptype) {
        default:
            break;

        case PT_CHAT: {
            if (mbuf.size > 0 && CLIENT.show_user_messages)
                chat_message(fmt("%s: %.*s", get_peer_name(msg.from), mbuf.size, mbuf.data), B_WHITE);
            break;
        }

        case PT_BAIL: {
            if (nuke_spectator_peer(msg.from))
                break;

            for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
                if (player_peers[i] != msg.from)
                    continue;

                if (get_screen() != SCR_MENU)
                    boot_to_menu(LFMT("msg_player_bailed", 's', get_peer_name(player_peers[i])));

                player_peers[i] = 0;
                break;
            }

            break;
        }

        case PT_WORLD: {
            INFO("World packet from %s (%" SDL_PRIu64 ")", get_peer_name(msg.from), msg.from);
            if (!i_am_in_tables()) {
                WTF("Not in player peer table, ignoring");
                break;
            }

            WorldContext ctx = empty_world_context();

            read_buffer64(&mbuf, &ctx.world);
            const World* world = get_world_key(ctx.world);
            if (world == NULL) {
                WTF("Invalid world key %" SDL_PRIu64, ctx.world);
                break;
            }

            read_buffer8(&mbuf, &ctx.level);
            read_buffer16(&mbuf, &ctx.flags);

            read_buffer8(&mbuf, (Uint8*)&ctx.winner);
            if (ctx.winner < 0 || ctx.winner >= SDL_arraysize(ctx.players)) {
                WTF("Bad world winner (%i)", ctx.winner);
                break;
            }

            read_buffer8(&mbuf, (Uint8*)&ctx.num_players);
            if (ctx.num_players <= 0 || ctx.num_players > SDL_arraysize(ctx.players)) {
                WTF("Bad world player count (%i)", ctx.num_players);
                break;
            }

            for (PlayerID i = 0; i < ctx.num_players; i++) {
                read_buffer8(&mbuf, &ctx.players[i].character);
                read_buffer8(&mbuf, &ctx.players[i].powerup);
                read_buffer8(&mbuf, (Uint8*)&ctx.players[i].lives);
                read_buffer8(&mbuf, &ctx.players[i].coins);
                read_buffer32(&mbuf, (Uint32*)&ctx.players[i].score);
            }

            if (world->has_map)
                set_screen(SCR_MAP, &ctx, sizeof(ctx));
            else
                start_world(&ctx);

            break;
        }

        case PT_GAME: {
            INFO("Game packet from %s (%" SDL_PRIu64 ")", get_peer_name(msg.from), msg.from);
            if (!i_am_in_tables()) {
                WTF("Not in player peer table, ignoring");
                break;
            }

            GameContext ctx = empty_game_context();

            read_buffer16(&mbuf, &ctx.flags);

            read_buffer64(&mbuf, &ctx.level);
            const Level* level = get_level_key(ctx.level);
            if (level == NULL) {
                WTF("Invalid level key %" SDL_PRIu64, ctx.level);
                break;
            }

            read_buffer16(&mbuf, (Uint16*)&ctx.checkpoint);

            read_buffer8(&mbuf, (Uint8*)&ctx.num_players);
            if (ctx.num_players <= 0 || ctx.num_players > SDL_arraysize(ctx.players)) {
                WTF("Bad game player count (%i)", ctx.num_players);
                break;
            }

            for (PlayerID i = 0; i < ctx.num_players; i++) {
                read_buffer8(&mbuf, &ctx.players[i].xscroll);
                read_buffer8(&mbuf, &ctx.players[i].character);
                read_buffer8(&mbuf, &ctx.players[i].powerup);
                read_buffer8(&mbuf, (Uint8*)&ctx.players[i].lives);
                read_buffer8(&mbuf, &ctx.players[i].coins);
                read_buffer32(&mbuf, (Uint32*)&ctx.players[i].score);
            }

            set_screen(SCR_GAME, &ctx, sizeof(ctx));
            break;
        }

        case PT_PLAYERS: {
            const NetID me = get_local_peer();

            Bool was_player = FALSE;
            for (size_t i = 0; i < SDL_arraysize(player_peers); i++) {
                if (me == player_peers[i]) {
                    was_player = TRUE;
                    break;
                }
            }
            if (!was_player) {
                for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++) {
                    if (me == spectator_peers[i]) {
                        was_player = TRUE;
                        break;
                    }
                }
            }

            clear_player_peer_tables();

            Uint8 num_players = 0;
            read_buffer8(&mbuf, &num_players);
            if (num_players <= 0 || num_players > SDL_arraysize(player_peers)) {
                WTF("Bad peer player count (%i)", num_players);
                break;
            }

            Uint8 num_spectators = 0;
            read_buffer8(&mbuf, &num_spectators);
            if (num_spectators < 0 || num_spectators > SDL_arraysize(spectator_peers)) {
                WTF("Bad peer spectator count (%i)", num_spectators);
                break;
            }

            for (Uint8 i = 0; i < num_players; i++)
                read_buffer64(&mbuf, &player_peers[i]);

            for (Uint8 i = 0; i < num_spectators; i++)
                read_buffer64(&mbuf, &spectator_peers[i]);

            if (was_player && get_screen() != SCR_MENU)
                boot_to_menu(LFMT("msg_kicked_from_game"));

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
    return connect_state == CONN_CONNECTED;
}

Bool is_host() {
    return !is_connected() || get_master_peer() == get_local_peer();
}

Bool is_client() {
    return !is_host();
}

Bool is_leader() {
    return get_leading_peer() == get_local_peer();
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
    return is_connected() ? NutBlast_GetOurID() : 0;
}

NetID get_master_peer() {
    return NutBlast_GetMasterID();
}

NetID get_leading_peer() {
    for (size_t i = 0; i < SDL_arraysize(player_peers); i++)
        if (player_peers[i] > 0)
            return player_peers[i];

    return get_master_peer();
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

Sint64 get_peer_number(NetID pid, const char* key) {
    const char* data = NutBlast_GetPlayerField(pid, key);
    return (data == NULL) ? 0 : SDL_atoi(data);
}

Bool get_peer_bool(NetID pid, const char* key) {
    return get_peer_number(pid, key) > 0;
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

void bail_from_game() {
    spread_reliable_packet_to_players(PCH_LOBBY, &(PacketType){PT_BAIL}, sizeof(PacketType));
    clear_player_peer_tables();
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
    for (const NetID* pids = get_peers(); *pids > 0; pids++)
        if (!get_peer_bool(*pids, "spectator"))
            ++count;

    return count;
}

Uint8 get_lobby_spectator_count() {
    Uint8 count = 0;
    for (const NetID* pids = get_peers(); *pids > 0; pids++)
        if (get_peer_bool(*pids, "spectator"))
            ++count;

    return count;
}

Uint8 get_game_player_count() {
    Uint8 count = 0;
    for (Uint8 i = 0; i < (Uint8)SDL_arraysize(player_peers); i++)
        if (player_peers[i] > 0)
            ++count;

    return count;
}

Uint8 get_game_spectator_count() {
    Uint8 count = 0;
    for (Uint8 i = 0; i < (Uint8)SDL_arraysize(spectator_peers); i++)
        if (spectator_peers[i] > 0)
            ++count;

    return count;
}

Bool i_am_spectating() {
    if (!is_connected())
        return FALSE;

    const NetID me = get_local_peer();

    for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++)
        if (me == spectator_peers[i])
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
    NutBlast_SetPlayerField("spectator", "0"); // GROSS HACK: pre-set `spectator` to trigger diff the first time.
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
    if (connect_state == CONN_CONNECTING)
        WTF("Connect state ended in failure");
    connect_state = CONN_DISCONNECTED;

    if (!from_on_disconnected) {
        NutBlast_Disconnect();
        clear_lobby_list();
    }

    if (last_error == NULL) {
        if (get_screen() != SCR_MENU)
            boot_to_menu(LFMT("msg_disconnected"));
    } else {
        WARN("Disconnected with error: %s", last_error);
        if (get_screen() != SCR_MENU)
            boot_to_menu(fmt("%s\n%s", LFMT("msg_disconnected"), last_error));
    }

    NutBlast_PurgeMetadata();
    clear_player_peer_tables();
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

Sint64 get_lobby_number(const char* key) {
    const char* data = get_lobby_string(key);
    return (data == NULL) ? 0 : SDL_atoi(data);
}

Bool get_lobby_bool(const char* key) {
    return get_lobby_number(key) > 0;
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
    const NetID me = get_local_peer();

    const char* field = get_peer_name(me);
    if (field == NULL || SDL_strcmp(CLIENT.name, field) != 0)
        NutBlast_SetPlayerField(NUTBLAST_FIELD_PLAYER_NAME, CLIENT.name);

    if (CLIENT.xscroll != get_peer_number(me, "xscroll"))
        NutBlast_SetPlayerField("xscroll", fmt("%u", CLIENT.xscroll));

    if (CLIENT.character != get_peer_number(me, "character"))
        NutBlast_SetPlayerField("character", fmt("%u", CLIENT.character));

    if (CLIENT.powerup != get_peer_number(me, "powerup"))
        NutBlast_SetPlayerField("powerup", fmt("%u", CLIENT.powerup));
}

void peers_to_players() {
    clear_player_peer_tables();

    Uint8 num_players = 0, num_spectators = 0;

    // Fill player-to-peer and spectator-to-peer tables
    for (const NetID* pids = get_peers(); *pids > 0; pids++) {
        const NetID pid = *pids;
        if (num_players >= SDL_arraysize(player_peers) || get_peer_bool(pid, "spectator")) {
            if (num_spectators < SDL_arraysize(spectator_peers))
                spectator_peers[num_spectators++] = pid;
        } else {
            if (num_players < SDL_arraysize(player_peers))
                player_peers[num_players++] = pid;
        }
    }

    // Send table to peers
    Buffer buffer = net_buffer();
    write_buffer8(&buffer, &(PacketType){PT_PLAYERS});

    write_buffer8(&buffer, &num_players);
    write_buffer8(&buffer, &num_spectators);

    for (Uint8 i = 0; i < num_players; i++)
        write_buffer64(&buffer, &player_peers[i]);

    for (Uint8 i = 0; i < num_spectators; i++)
        write_buffer64(&buffer, &spectator_peers[i]);

    spread_reliable_packet(PCH_LOBBY, buffer.data, buffer_tell(buffer));
}

static void net_send(GekkoNetAddress* gn_addr, const char* data, int len) {
    NutBlast_SendTo(PCH_GAME, *(NetID*)gn_addr->data, data, len);
}

static GekkoNetResult** net_receive(int* pcount) {
    static GekkoNetResult* packets[MAX_GAME_PACKETS] = {0};
    *pcount = 0;

    NutBlast_Message msg = {0};
    while (NutBlast_NextMessage(PCH_GAME, &msg)) {
        GekkoNetResult* res = SDL_malloc(sizeof(*res));
        if (res == NULL)
            break;

        res->addr.size = sizeof(NetID);
        res->addr.data = SDL_malloc(sizeof(NetID));
        if (res->addr.data == NULL) {
            SDL_free(res);
            break;
        }
        *(NetID*)res->addr.data = msg.from;

        res->data_len = msg.size;
        res->data = SDL_malloc(msg.size);
        if (res->data == NULL) {
            SDL_free(res->addr.data);
            SDL_free(res);
            break;
        }
        SDL_memcpy(res->data, msg.data, msg.size);

        packets[(*pcount)++] = res;
        if (*pcount >= MAX_GAME_PACKETS)
            break;
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

        return 0;
    }

    gekko_net_adapter_set(session, &adapter);

    if (i_am_spectating()) {
        GekkoNetAddress addr = {0};
        addr.data = &player_peers[0];
        addr.size = sizeof(*player_peers);
        gekko_add_actor(session, GekkoRemotePlayer, &addr);

        return MAX_PLAYERS;
    }

    // Fill players
    INFO("PLAYERS:");

    PlayerID local = MAX_PLAYERS, tv = MAX_PLAYERS;
    const NetID me = get_local_peer();
    for (PlayerID i = 0; i < (PlayerID)SDL_arraysize(player_peers); i++) {
        if (player_peers[i] <= 0)
            continue;

        if (me == player_peers[i]) {
            local = i;
            gekko_add_actor(session, GekkoLocalPlayer, NULL);
            gekko_set_local_delay(session, local, CLIENT.input_delay);
        } else {
            GekkoNetAddress addr = {0};
            addr.data = &player_peers[i];
            addr.size = sizeof(*player_peers);
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

        for (Uint8 i = 0; i < (Uint8)SDL_arraysize(spectator_peers); i++) {
            if (spectator_peers[i] <= 0)
                continue;

            GekkoNetAddress addr = {0};
            addr.data = &spectator_peers[i];
            addr.size = sizeof(*spectator_peers);
            gekko_add_actor(session, GekkoSpectator, &addr);

            INFO("- %s", get_peer_name(spectator_peers[i]));
        }
    }

    return (PlayerID)local;
}

Buffer net_buffer() {
    static Uint8 data[NET_BUFFER_SIZE] = {0};
    return buffer_from(data, sizeof(data));
}

void send_packet(PacketChannel channel, NetID pid, const Uint8* data, size_t size) {
    NutBlast_SendTo(channel, pid, (char*)data, (int)size);
}

void send_reliable_packet(PacketChannel channel, NetID pid, const Uint8* data, size_t size) {
    NutBlast_SendReliablyTo(channel, pid, (char*)data, (int)size);
}

void spread_packet(PacketChannel channel, const Uint8* data, size_t size) {
    for (const NetID* pids = get_peers(); *pids > 0; pids++)
        send_packet(channel, *pids, data, size);
}

void spread_reliable_packet(PacketChannel channel, const Uint8* data, size_t size) {
    for (const NetID* pids = get_peers(); *pids > 0; pids++)
        send_reliable_packet(channel, *pids, data, size);
}

void spread_reliable_packet_to_players(PacketChannel channel, const Uint8* data, size_t size) {
    for (size_t i = 0; i < SDL_arraysize(player_peers); i++)
        send_reliable_packet(channel, player_peers[i], data, size);
    for (size_t i = 0; i < SDL_arraysize(spectator_peers); i++)
        send_reliable_packet(channel, spectator_peers[i], data, size);
}

void spread_world_packet(const WorldContext* ctx) {
    if (ctx == NULL)
        return;

    Buffer buffer = net_buffer();
    write_buffer8(&buffer, &(PacketType){PT_WORLD});

    write_buffer64(&buffer, &ctx->world);
    write_buffer8(&buffer, &ctx->level);
    write_buffer16(&buffer, &ctx->flags);

    write_buffer8(&buffer, (Uint8*)&ctx->winner);
    write_buffer8(&buffer, (Uint8*)&ctx->num_players);
    for (PlayerID i = 0; i < ctx->num_players; i++) {
        write_buffer8(&buffer, &ctx->players[i].character);
        write_buffer8(&buffer, &ctx->players[i].powerup);
        write_buffer8(&buffer, (Uint8*)&ctx->players[i].lives);
        write_buffer8(&buffer, &ctx->players[i].coins);
        write_buffer32(&buffer, (Uint32*)&ctx->players[i].score);
    }

    spread_reliable_packet_to_players(PCH_LOBBY, buffer.data, buffer_tell(buffer));
}

void spread_game_packet(const GameContext* ctx) {
    if (ctx == NULL)
        return;

    Buffer buffer = net_buffer();
    write_buffer8(&buffer, &(PacketType){PT_GAME});

    write_buffer16(&buffer, &ctx->flags);
    write_buffer64(&buffer, &ctx->level);
    write_buffer16(&buffer, (Uint16*)&ctx->checkpoint);

    write_buffer8(&buffer, (Uint8*)&ctx->num_players);
    for (PlayerID i = 0; i < ctx->num_players; i++) {
        write_buffer8(&buffer, &ctx->players[i].xscroll);
        write_buffer8(&buffer, &ctx->players[i].character);
        write_buffer8(&buffer, &ctx->players[i].powerup);
        write_buffer8(&buffer, (Uint8*)&ctx->players[i].lives);
        write_buffer8(&buffer, &ctx->players[i].coins);
        write_buffer32(&buffer, (Uint32*)&ctx->players[i].score);
    }

    spread_reliable_packet_to_players(PCH_LOBBY, buffer.data, buffer_tell(buffer));
}
