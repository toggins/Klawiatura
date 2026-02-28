#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_SNPrintF SDL_snprintf
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free
#define NutPunch_Log INFO
#define ptrdiff_t int64_t // HACK: solves the linux ci build failing

#include "K_chat.h"
#include "K_discord.h"
#include "K_log.h"
#include "K_menu.h"
#include "K_misc.h"
#include "K_net.h"

static char hostname[256] = "";
static const char* last_error = NULL;

static char cur_lobby[LOBBY_STRING_MAX] = "";

static int player_peers[MAX_PLAYERS] = {MAX_PEERS};
static int spectator_peers[MAX_PLAYERS] = {MAX_PEERS};

void net_init() {
	if (!hostname[0])
		set_hostname(NUTPUNCH_DEFAULT_SERVER);
	NutPunch_SetChannelCount(PCH_MAX);
}

void net_teardown() {
	NutPunch_Cleanup();
}

/// Returns the current hostname.
///
/// If the hostname is an IPv4 or IPv6 address, returns `(custom IP)` instead.
const char* get_hostname() {
	int count = 0;

	char* sep = SDL_strchr(hostname, '.');
	while (sep != NULL) {
		++count;
		sep = SDL_strchr(sep + 1, '.');
	}

	if (count < 3) { // Might be IPv6 then
		count = 0;
		char* sep = SDL_strchr(hostname, ':');
		while (sep != NULL) {
			++count;
			sep = SDL_strchr(sep + 1, ':');
		}
	}

	return count >= 3 ? "(custom IP)" : hostname;
}

void set_hostname(const char* hn) {
	SDL_snprintf(hostname, sizeof(hostname), "%s", hn);
	NutPunch_SetServerAddr(hostname);
}

Bool in_public_server() {
	return !SDL_strcmp(hostname, NUTPUNCH_DEFAULT_SERVER);
}

// =========
// INTERFACE
// =========

static void np_set_string(void setter(const char*, int, const void*), const char* name, const char* str) {
	static char buf[NUTPUNCH_FIELD_DATA_MAX] = {0};
	SDL_zeroa(buf), SDL_strlcpy(buf, str, sizeof(buf));
	(setter)(name, sizeof(buf), buf);
}
static void np_lobby_set_string(const char* name, const char* str) {
	return np_set_string(NutPunch_LobbySet, name, str);
}
static void np_peer_set_string(const char* name, const char* str) {
	return np_set_string(NutPunch_PeerSet, name, str);
}

// NOLINTBEGIN(bugprone-macro-parentheses)
#define MAKE_LOBBY_GETTER(suffix, type)                                                                                \
	static void np_lobby_get_##suffix(const char* name, type* dest) {                                              \
		int size = 0;                                                                                          \
		const type* value = NutPunch_LobbyGet(name, &size);                                                    \
		if (value != NULL && size == sizeof(type))                                                             \
			*dest = *value;                                                                                \
	}
// NOLINTEND(bugprone-macro-parentheses)

MAKE_LOBBY_GETTER(bool, Bool);
MAKE_LOBBY_GETTER(u8, Uint8);
MAKE_LOBBY_GETTER(u32, Uint32);
MAKE_LOBBY_GETTER(i8, Sint8);
#undef MAKE_LOBBY_GETTER

static void np_lobby_get_string(const char* name, char dest[NUTPUNCH_FIELD_DATA_MAX]) {
	int size = 0;
	const char* str = NutPunch_LobbyGet(name, &size);
	if (str != NULL && size == NUTPUNCH_FIELD_DATA_MAX)
		SDL_memcpy(dest, str, NUTPUNCH_FIELD_DATA_MAX);
}

static const char* verb = NULL;
const char* net_verb() {
	return verb;
}

void net_newframe() {
	if (NutPunch_Update() == NPS_Error) {
		last_error = NutPunch_GetLastError();
		disconnect();
		if (game_exists()) {
			show_error("Net error:\n%s", last_error);
			nuke_game();
		}
	} else
		last_error = NULL;

	char* data = net_buffer();
	while (NutPunch_HasMessage(PCH_LOBBY)) {
		int size = NET_BUFFER_SIZE, peer = NutPunch_NextMessage(PCH_LOBBY, data, &size);
		if (size < sizeof(PacketType))
			continue;

		switch (*(PacketType*)data) {
		default:
			break;

		case PT_START: {
			if (!NutPunch_IsMaster())
				start_online_game(TRUE);
			break;
		}

		case PT_CONTINUE: {
			if (NutPunch_IsMaster())
				break;

			GameContext* ctx = init_game_context();
			char* cursor = data;

			cursor += sizeof(PacketType);
			SDL_strlcpy(ctx->level, cursor, CLIENT_STRING_MAX), cursor += CLIENT_STRING_MAX;
			ctx->flags = *(GameFlag*)cursor, cursor += sizeof(GameFlag);
			ctx->checkpoint = *(ActorID*)cursor, cursor += sizeof(ActorID);

			ctx->num_players = *(PlayerID*)cursor, cursor += sizeof(PlayerID);
			for (PlayerID i = 0; i < ctx->num_players; i++) {
				ctx->players[i].lives = *(Sint8*)cursor, cursor += sizeof(Sint8);
				ctx->players[i].power = *(PlayerPower*)cursor, cursor += sizeof(PlayerPower);
				ctx->players[i].coins = *(Uint8*)cursor, cursor += sizeof(Uint8);
				ctx->players[i].score = *(Uint32*)cursor, cursor += sizeof(Uint32);
			}

			start_online_game(FALSE);
			break;
		}

		case PT_CHAT: {
			if (size > sizeof(PacketType))
				push_chat_message(peer, data + sizeof(PacketType), size - (int)sizeof(PacketType));
			break;
		}
		}
	}
}

const char* net_error() {
	return last_error;
}

void disconnect() {
	NutPunch_Disconnect();
	SDL_zeroa(cur_lobby);
	if (net_error())
		WARN("Net error: %s", net_error());
	update_discord_status(NULL);
	purge_chat();
}

void push_user_data() {
	np_peer_set_string("NAME", CLIENT.user.name);
}

// =======
// LOBBIES
// =======

const char* get_lobby_id() {
	return cur_lobby[0] == '\0' ? NULL : cur_lobby;
}

void host_lobby(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));
	NutPunch_Host(cur_lobby, CLIENT.game.players);
	verb = "host";
	push_user_data();

	NutPunch_LobbySet(GAME_NAME, sizeof(GAME_VERSION), GAME_VERSION);
	const Uint32 party = SDL_rand_bits(); // Generate a better UUID.
	NutPunch_LobbySet("PARTY", sizeof(party), &party);
	const LobbyVisibility visible = CLIENT.lobby.public ? VIS_PUBLIC : VIS_UNLISTED;
	NutPunch_LobbySet("VISIBLE", sizeof(visible), &visible);
	push_lobby_data();
}

void join_lobby(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));
	NutPunch_Join(cur_lobby);
	verb = "join";
	push_user_data();
}

void list_lobbies() {
	SDL_zeroa(cur_lobby);

	NutPunch_Filter filter[2] = {0};
	SDL_memcpy(filter[0].field.name, GAME_NAME, SDL_strnlen(GAME_NAME, NUTPUNCH_FIELD_NAME_MAX));
	SDL_memcpy(filter[0].field.value, GAME_VERSION, sizeof(GAME_VERSION));
	filter[0].comparison = NPF_Eq;

	SDL_memcpy(filter[1].field.name, "VISIBLE", SDL_strnlen("VISIBLE", NUTPUNCH_FIELD_NAME_MAX));
	const LobbyVisibility visible = VIS_PUBLIC;
	SDL_memcpy(filter[1].field.value, &visible, sizeof(visible));
	filter[1].comparison = NPF_Eq;

	NutPunch_FindLobbies(2, filter);
}

Bool in_public_lobby() {
	LobbyVisibility visible = 0;
	np_lobby_get_u8("VISIBLE", &visible);
	return visible == VIS_PUBLIC;
}

void push_lobby_data() {
	NutPunch_LobbySet("KEVIN", sizeof(CLIENT.game.kevin), &CLIENT.game.kevin);
	NutPunch_LobbySet("FRED", sizeof(CLIENT.game.fred), &CLIENT.game.fred);
	np_lobby_set_string("LEVEL", CLIENT.game.level);
}

void pull_lobby_data() {
	np_lobby_get_bool("KEVIN", &CLIENT.game.kevin);
	np_lobby_get_bool("FRED", &CLIENT.game.fred);
	np_lobby_get_string("LEVEL", CLIENT.game.level);
}

Uint32 get_lobby_party() {
	Uint32 party = 0;
	np_lobby_get_u32("PARTY", &party);
	return party;
}

// =====
// PEERS
// =====

int player_to_peer(PlayerID pid) {
	return (pid < 0L || pid >= MAX_PLAYERS) ? MAX_PEERS : player_peers[pid];
}

int spectator_to_peer(int sid) {
	return (sid < 0L || sid >= MAX_PLAYERS) ? MAX_PEERS : spectator_peers[sid];
}

const char* get_peer_name(int idx) {
	int size = 0;
	char* str = (char*)NutPunch_PeerGet(idx, "NAME", &size);
	return (str == NULL || size > NUTPUNCH_FIELD_DATA_MAX) ? NULL : str;
}

Bool peer_is_spectating(int idx) {
	int size = 0;
	Bool* spec = (Bool*)NutPunch_PeerGet(idx, "SPEC", &size);
	return (spec == NULL || size < sizeof(Bool)) ? FALSE : *spec;
}

Bool i_am_spectating() {
	return peer_is_spectating(NutPunch_LocalPeer());
}

void start_online_game(Bool override_ctx) {
	pull_lobby_data();

	if (!override_ctx)
		goto dont_override_ctx;

	int num_players = 0;
	for (int i = 0; i < MAX_PEERS; i++)
		if (NutPunch_PeerAlive(i) && !peer_is_spectating(i))
			++num_players;
	if (num_players < 1) {
		WTF("Not enough players");
		return;
	}

	play_generic_sound("enter");

	GameContext* ctx = init_game_context();
	SDL_strlcpy(ctx->level, CLIENT.game.level, sizeof(ctx->level));
	ctx->flags |= GF_TRY_HELL;
	ctx->num_players = (PlayerID)num_players;

dont_override_ctx:
	start_game();
}

static void net_send(GekkoNetAddress* gn_addr, const char* data, int len) {
	NutPunch_Send(PCH_GAME, *(int*)gn_addr->data, data, len);
}

static GekkoNetResult** net_receive(int* packet_count) {
	static GekkoNetResult* packets[64] = {0};

	char* data = net_buffer();
	*packet_count = 0;

	while (NutPunch_HasMessage(PCH_GAME) && *packet_count < entries(packets)) {
		int size = NET_BUFFER_SIZE, peer = NutPunch_NextMessage(PCH_GAME, data, &size);
		GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));

		res->addr.size = sizeof(int), res->addr.data = SDL_malloc(res->addr.size);
		*(int*)res->addr.data = peer;

		res->data_len = size, res->data = SDL_malloc(size);
		SDL_memcpy(res->data, data, size);

		packets[(*packet_count)++] = res;
	}

	return packets;
}

PlayerID populate_game(GekkoSession* session) {
	static GekkoNetAdapter adapter = {0};
	adapter.send_data = net_send;
	adapter.receive_data = net_receive;
	adapter.free_data = SDL_free;
	gekko_net_adapter_set(session, &adapter);

	for (int i = 0; i < MAX_PLAYERS; i++) {
		player_peers[i] = MAX_PEERS;
		spectator_peers[i] = MAX_PEERS;
	}

	if (!NutPunch_IsReady()) {
		gekko_add_actor(session, GekkoLocalPlayer, NULL);
		return 0;
	}

	const int num_peers = NutPunch_PeerCount();
	int counter = 0, local = MAX_PLAYERS, tv = MAX_PLAYERS;
	for (int i = 0; i < MAX_PEERS; i++) {
		if (!NutPunch_PeerAlive(i) || peer_is_spectating(i))
			continue;

		player_peers[counter] = i;
		if (NutPunch_LocalPeer() == i) {
			local = counter;
			gekko_add_actor(session, GekkoLocalPlayer, NULL);
			gekko_set_local_delay(session, local, CLIENT.input.delay);
			INFO("You are player %i", local + 1);
		} else {
			GekkoNetAddress addr = {0};
			addr.size = sizeof(*player_peers);
			addr.data = &player_peers[counter];
			gekko_add_actor(session, GekkoRemotePlayer, &addr);
		}

		// First valid player will act as a host for all spectators
		if (tv >= MAX_PLAYERS)
			tv = counter;

		if (++counter >= num_peers)
			break;
	}

	if (tv != local || i_am_spectating())
		goto finish_populating;

	int num_spectators = 0;
	for (int i = 0; i < MAX_PEERS; i++) {
		if (!NutPunch_PeerAlive(i) || !peer_is_spectating(i))
			continue;

		spectator_peers[counter] = i;
		GekkoNetAddress addr = {0};
		addr.size = sizeof(*spectator_peers);
		addr.data = &spectator_peers[counter];
		gekko_add_actor(session, GekkoSpectator, &addr);
		++num_spectators;

		if (++counter >= num_peers)
			break;
	}
	if (num_spectators > 0)
		INFO("You have %i spectator(s)", num_spectators);

finish_populating:
	return (PlayerID)local;
}

// ======
// BUFFER
// ======

char* net_buffer() {
	static char buf[NET_BUFFER_SIZE] = {0};
	return buf;
}
