#include "K_log.h"

#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_SNPrintF SDL_snprintf
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free
#define NutPunch_Log INFO

#include "K_cmd.h"
#include "K_discord.h"
#include "K_game.h"
#include "K_net.h"
#include "K_tick.h"

static char hostname[512] = "";
static const char* last_error = NULL;

static char cur_lobby[NUTPUNCH_ID_MAX] = "";
static enum {
	NET_NULL,
	NET_HOST,
	NET_JOIN,
	NET_LIST,
} netmode = NET_NULL;

static const char* MAGIC_KEY = "KLAWIATURA";
static const uint8_t MAGIC_VALUE = 127;

static int player_peers[MAX_PLAYERS] = {MAX_PEERS};

void net_init() {
	if (!hostname[0])
		set_hostname(NUTPUNCH_DEFAULT_SERVER);
}

void net_teardown() {
	disconnect();
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

bool in_public_server() {
	return !SDL_strcmp(hostname, NUTPUNCH_DEFAULT_SERVER);
}

// =========
// INTERFACE
// =========

#define np_lobby_set NutPunch_LobbySet
#define np_peer_set NutPunch_PeerSet

static void np_set_string(void setter(const char*, int, const void*), const char* name, const char* str) {
	static char buf[NUTPUNCH_FIELD_DATA_MAX] = {0};
	SDL_memset(buf, 0, sizeof(buf)), SDL_strlcpy(buf, str, sizeof(buf));
	(setter)(name, sizeof(buf), buf);
}
static void np_lobby_set_string(const char* name, const char* str) {
	return np_set_string(np_lobby_set, name, str);
}
static void np_peer_set_string(const char* name, const char* str) {
	return np_set_string(np_peer_set, name, str);
}

#define MAKE_LOBBY_GETTER(suffix, type)                                                                                \
	static void np_lobby_get_##suffix(const char* name, type* dest) {                                              \
		int size = 0;                                                                                          \
		type* value = NutPunch_LobbyGet(name, &size);                                                          \
		if (value != NULL && size == sizeof(type))                                                             \
			*dest = *value;                                                                                \
	}
MAKE_LOBBY_GETTER(bool, bool);
MAKE_LOBBY_GETTER(u8, uint8_t);
MAKE_LOBBY_GETTER(u32, uint32_t);
MAKE_LOBBY_GETTER(i8, int8_t);
#undef MAKE_LOBBY_GETTER

static void np_lobby_get_string(const char* name, char dest[NUTPUNCH_FIELD_DATA_MAX]) {
	int size = 0;
	char* str = NutPunch_LobbyGet(name, &size);
	if (str != NULL && size == NUTPUNCH_FIELD_DATA_MAX)
		SDL_memcpy(dest, str, NUTPUNCH_FIELD_DATA_MAX);
}

void net_newframe() {
	if (is_connected()) {
		push_user_data();
		if (is_host())
			push_lobby_data();
	}

	if (NutPunch_Update() == NPS_Error) {
		last_error = NutPunch_GetLastError();
		disconnect();
	}

	if (is_connected() && is_client())
		pull_lobby_data();
}

const char* net_error() {
	return last_error;
}

bool is_connected() {
	return netmode != NET_NULL && cur_lobby[0] != '\0' && NutPunch_PeerCount() >= 1;
}

void disconnect() {
	NutPunch_Disconnect();
	SDL_memset(cur_lobby, 0, sizeof(cur_lobby));
	netmode = NET_NULL;
	if (net_error())
		WARN("Net error: %s", net_error());
	update_discord_status(NULL);
}

bool is_host() {
	return NutPunch_IsMaster();
}

bool is_client() {
	return !NutPunch_IsMaster();
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
	netmode = NET_HOST;
	push_user_data();

	np_lobby_set(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
	const uint32_t party = SDL_rand_bits(); // Generate a better UUID.
	np_lobby_set("PARTY", sizeof(party), &party);
	const LobbyVisibility visible = CLIENT.lobby.public ? VIS_PUBLIC : VIS_UNLISTED;
	np_lobby_set("VISIBLE", sizeof(visible), &visible);
	push_lobby_data();
}

void join_lobby(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));
	NutPunch_Join(cur_lobby);
	netmode = NET_JOIN;
	push_user_data();
}

void list_lobbies() {
	SDL_memset(cur_lobby, 0, sizeof(cur_lobby));

	NutPunch_Filter filter[2] = {0};
	SDL_memcpy(filter[0].field.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));
	SDL_memcpy(filter[0].field.value, &MAGIC_VALUE, sizeof(MAGIC_VALUE));
	filter[0].comparison = NPF_Eq;

	SDL_memcpy(filter[1].field.name, "VISIBLE", SDL_strnlen("VISIBLE", NUTPUNCH_FIELD_NAME_MAX));
	const LobbyVisibility visible = VIS_PUBLIC;
	SDL_memcpy(filter[1].field.value, &visible, sizeof(visible));
	filter[1].comparison = NPF_Eq;

	NutPunch_FindLobbies(2, filter);
	netmode = NET_LIST;
}

int get_lobby_count() {
	return NutPunch_LobbyCount();
}

const NutPunch_LobbyInfo* get_lobby(int idx) {
	return NutPunch_GetLobby(idx);
}

bool in_public_lobby() {
	LobbyVisibility visible = 0;
	np_lobby_get_u8("VISIBLE", &visible);
	return visible == VIS_PUBLIC;
}

void push_lobby_data() {
	np_lobby_set("KEVIN", sizeof(CLIENT.game.kevin), &CLIENT.game.kevin);
	np_lobby_set("FRED", sizeof(CLIENT.game.fred), &CLIENT.game.fred);
	np_lobby_set_string("LEVEL", CLIENT.game.level);
}

void pull_lobby_data() {
	np_lobby_get_bool("KEVIN", &CLIENT.game.kevin);
	np_lobby_get_bool("FRED", &CLIENT.game.fred);
	np_lobby_get_string("LEVEL", CLIENT.game.level);
}

void make_lobby_active() {
	const LobbyVisibility visible = VIS_INVALID;
	np_lobby_set("VISIBLE", sizeof(visible), &visible);
}

uint32_t get_lobby_party() {
	uint32_t party = 0;
	np_lobby_get_u32("PARTY", &party);
	return party;
}

// =====
// PEERS
// =====

int get_peer_count() {
	return NutPunch_PeerCount();
}

int get_max_peers() {
	return NutPunch_GetMaxPlayers();
}

bool peer_exists(int idx) {
	return NutPunch_PeerAlive(idx);
}

int player_to_peer(PlayerID pid) {
	return (pid < 0L || pid >= MAX_PLAYERS) ? MAX_PEERS : player_peers[pid];
}

const char* get_peer_name(int idx) {
	int size = 0;
	char* str = (char*)NutPunch_PeerGet(idx, "NAME", &size);
	return (str == NULL || size > NUTPUNCH_FIELD_DATA_MAX) ? NULL : str;
}

static void net_send(GekkoNetAddress* gn_addr, const char* data, int len) {
	NutPunch_Send(*(int*)gn_addr->data, data, len);
}

static GekkoNetResult** net_receive(int* pCount) {
	static GekkoNetResult* packets[64] = {0};
	static char data[NUTPUNCH_BUFFER_SIZE] = {0};
	*pCount = 0;

	while (NutPunch_HasMessage() && *pCount < sizeof(packets) / sizeof(void*)) {
		int size = sizeof(data), peer = NutPunch_NextMessage(data, &size);
		if (!SDL_memcmp(data, "CHAT", 4)) { // GROSS HACK
			push_chat_message(peer, data + 4);
			continue;
		}

		GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));

		res->addr.size = sizeof(int), res->addr.data = SDL_malloc(res->addr.size);
		*(int*)res->addr.data = peer;

		res->data_len = size, res->data = SDL_malloc(size);
		SDL_memcpy(res->data, data, size);

		packets[*pCount] = res, *pCount += 1;
	}

	return packets;
}

PlayerID populate_game(GekkoSession* session) {
	static GekkoNetAdapter adapter = {0};
	adapter.send_data = net_send;
	adapter.receive_data = net_receive;
	adapter.free_data = SDL_free;
	gekko_net_adapter_set(session, &adapter);

	for (int i = 0; i < MAX_PLAYERS; i++)
		player_peers[i] = MAX_PEERS;

	const int num_peers = NutPunch_PeerCount();
	if (num_peers <= 1) {
		gekko_add_actor(session, LocalPlayer, NULL);
		return 0;
	}

	int counter = 0, local = MAX_PLAYERS;
	for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++) {
		if (!NutPunch_PeerAlive(i))
			continue;

		player_peers[counter] = i;
		if (NutPunch_LocalPeer() == i) {
			local = counter;
			gekko_add_actor(session, LocalPlayer, NULL);
			gekko_set_local_delay(session, local, CLIENT.input.delay);
			INFO("You are player %i", local + 1);
		} else {
			GekkoNetAddress addr = {0};
			addr.size = sizeof(*player_peers);
			addr.data = &player_peers[counter];
			gekko_add_actor(session, RemotePlayer, &addr);
		}

		if (++counter >= num_peers)
			break;
	}

	return (PlayerID)local;
}
