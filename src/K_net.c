#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free

#include "K_cmd.h"
#include "K_log.h" // IWYU pragma: keep for now
#include "K_net.h"
#include "K_tick.h"

static const char *hostname = NUTPUNCH_DEFAULT_SERVER, *last_error = NULL;

static char cur_lobby[CLIENT_STRING_MAX] = "";
static enum {
	NET_NULL,
	NET_HOST,
	NET_JOIN,
	NET_LIST,
} netmode = NET_NULL;
static bool found_lobby = false;

static const char* MAGIC_KEY = "KLAWIATURA";
#define PUBLIC_LOBBY 127
#define PRIVATE_LOBBY 126
#define ACTIVE_LOBBY 0

void net_init() {
	NutPunch_SetServerAddr(hostname);
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
	NutPunch_SetServerAddr(hn);
	hostname = hn;
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
	found_lobby = false;
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

static int find_lobby_timeout = 0;
static void find_lobby_mode(const char* id) {
	if (id == NULL)
		SDL_memset(cur_lobby, 0, sizeof(cur_lobby));
	else
		SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));

	NutPunch_Filter filter = {0};
	SDL_memcpy(filter.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));

	uint8_t magic;
	if (id == NULL) {
		magic = PUBLIC_LOBBY;
		filter.comparison = NPF_Eq;
	} else {
		magic = ACTIVE_LOBBY;
		filter.comparison = NPF_Greater;
	}
	SDL_memcpy(filter.value, &magic, sizeof(magic));

	found_lobby = false;
	find_lobby_timeout = 5 * TICKRATE;
	NutPunch_FindLobbies(1, &filter);
}

void host_lobby(const char* id) {
	find_lobby_mode(id);
	netmode = NET_HOST;
}

void join_lobby(const char* id) {
	find_lobby_mode(id);
	netmode = NET_JOIN;
}

void list_lobbies() {
	find_lobby_mode(NULL);
	netmode = NET_LIST;
}

int find_lobby() {
	if (netmode == NET_NULL) {
		last_error = "Not connected to network";
		return -1;
	} else if (netmode == NET_LIST)
		return 0;
	else if (found_lobby) {
		push_user_data();
		if (netmode == NET_HOST)
			push_lobby_data();
		else if (netmode == NET_JOIN)
			pull_lobby_data();

		return NutPunch_PeerCount() >= 1;
	}

	for (int i = 0; i < NutPunch_LobbyCount(); i++) {
		if (SDL_strcmp(cur_lobby, NutPunch_GetLobby(i)))
			continue;
		if (netmode == NET_HOST) {
			last_error = "Lobby with this name exists";
			return -1;
		} else if (netmode == NET_JOIN) {
			NutPunch_Join(cur_lobby);
			pull_lobby_data();
			found_lobby = true;
			return 0;
		}
	}

	if (netmode == NET_HOST) {
		NutPunch_Host(cur_lobby);

		push_user_data();
		const uint8_t magic = CLIENT.lobby.public ? PUBLIC_LOBBY : PRIVATE_LOBBY;
		np_lobby_set(MAGIC_KEY, sizeof(magic), &magic);
		push_lobby_data();

		found_lobby = true;
		return 0;
	} else if (!found_lobby && netmode == NET_JOIN) {
		if (!NutPunch_LobbyCount() && find_lobby_timeout > 0)
			find_lobby_timeout--;
		if (!find_lobby_timeout) {
			last_error = "Lobby doesn't exist";
			return -1;
		}
	}

	return 0;
}

int get_lobby_count() {
	return NutPunch_LobbyCount();
}

const char* get_lobby(int idx) {
	return NutPunch_GetLobby(idx);
}

bool in_public_lobby() {
	uint8_t magic = 0;
	np_lobby_get_u8(MAGIC_KEY, &magic);
	return magic == PUBLIC_LOBBY;
}

void push_lobby_data() {
	np_lobby_set("PLAYERS", sizeof(CLIENT.game.players), &CLIENT.game.players);
	np_lobby_set("KEVIN", sizeof(CLIENT.game.kevin), &CLIENT.game.kevin);
	np_lobby_set_string("LEVEL", CLIENT.game.level);
}

void pull_lobby_data() {
	np_lobby_get_i8("PLAYERS", &CLIENT.game.players);
	np_lobby_get_bool("KEVIN", &CLIENT.game.kevin);
	np_lobby_get_string("LEVEL", CLIENT.game.level);
}

void make_lobby_active() {
	const uint8_t magic = ACTIVE_LOBBY;
	np_lobby_set(MAGIC_KEY, sizeof(magic), &magic);
}

// =====
// PEERS
// =====

int get_peer_count() {
	return NutPunch_PeerCount();
}

bool peer_exists(int idx) {
	return NutPunch_PeerAlive(idx);
}

const char* get_peer_name(int idx) {
	int size = 0;
	char* str = (char*)NutPunch_PeerGet(idx, "NAME", &size);
	return (str == NULL || size > NUTPUNCH_FIELD_DATA_MAX) ? NULL : str;
}

int8_t populate_game(GekkoSession* session) {
	// FIXME: Actually fill players.
	//        When the local peer is filled, give it 2 ticks of input delay here.
	gekko_add_actor(session, LocalPlayer, NULL);
	return 0;
}
