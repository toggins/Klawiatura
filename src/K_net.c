#include "K_log.h"

#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_SNPrintF SDL_snprintf
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free
#define NutPunch_Log INFO
#include <NutPunch.h>

#include "K_discord.h"
#include "K_menu.h"
#include "K_net.h"

static char hostname[256] = "";
static const char* last_error = NULL;

static char cur_lobby[LOBBY_STRING_MAX] = "";

static const char* MAGIC_KEY = "KLAWIATURA";
static const Uint8 MAGIC_VALUE = 127;

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

Bool in_public_server() {
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
}

const char* net_error() {
	return last_error;
}

Bool is_connected() {
	return cur_lobby[0] != '\0' && NutPunch_PeerCount() >= 1;
}

void disconnect() {
	NutPunch_Disconnect();
	SDL_memset(cur_lobby, 0, sizeof(cur_lobby));
	if (net_error())
		WARN("Net error: %s", net_error());
	update_discord_status(NULL);
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

	np_lobby_set(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
	const Uint32 party = SDL_rand_bits(); // Generate a better UUID.
	np_lobby_set("PARTY", sizeof(party), &party);
	const LobbyVisibility visible = CLIENT.lobby.public ? VIS_PUBLIC : VIS_UNLISTED;
	np_lobby_set("VISIBLE", sizeof(visible), &visible);
	push_lobby_data();
}

void join_lobby(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));
	NutPunch_Join(cur_lobby);
	verb = "join";
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
}

Bool in_public_lobby() {
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
