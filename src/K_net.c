#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free

#include "K_cmd.h"
#include "K_log.h" // IWYU pragma: keep for now
#include "K_net.h"

static const char *hostname = NUTPUNCH_DEFAULT_SERVER, *last_error = NULL;

static char cur_lobby[CLIENT_STRING_MAX] = "";
static enum {
	NET_NULL,
	NET_HOST,
	NET_JOIN,
	NET_LIST,
} netmode = NET_NULL;

static const char* MAGIC_KEY = "KLAWIATURA";
static const uint8_t MAGIC_VALUE = 127;

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

static void np_set_string(void setter(const char*, int, const void*), const char* name, const char* str) {
	(setter)(name, (int)SDL_strnlen(str, NUTPUNCH_FIELD_DATA_MAX - 1), str);
}
static void np_lobby_set_string(const char* name, const char* str) {
	return np_set_string(NutPunch_LobbySet, name, str);
}
static void np_peer_set_string(const char* name, const char* str) {
	return np_set_string(NutPunch_PeerSet, name, str);
}

#define MAKE_LOBBY_GETTER(suffix, type)                                                                                \
	static void np_lobby_get_##suffix(type* dest, const char* name) {                                              \
		int size = 0;                                                                                          \
		type* value = (type*)NutPunch_LobbyGet(name, &size);                                                   \
		if (value != NULL && size == sizeof(type))                                                             \
			*dest = *value;                                                                                \
	}
MAKE_LOBBY_GETTER(i8, int8_t);
#undef MAKE_GETTER

static void np_lobby_get_string(char* dest, const char* name) {
	int size = 0;
	char* str = (char*)NutPunch_LobbyGet(name, &size);
	if (str != NULL && size <= NUTPUNCH_FIELD_DATA_MAX)
		SDL_strlcpy(dest, str, size + 1);
}

extern ClientInfo CLIENT;
void net_newframe() {
	if (NutPunch_Update() == NP_Status_Error) {
		last_error = NutPunch_GetLastError();
		disconnect();
	}
	if (!is_connected())
		return;

	if (NutPunch_PeerAlive(NutPunch_LocalPeer()))
		np_peer_set_string("NAME", CLIENT.user.name);

	if (netmode == NET_HOST) {
		NutPunch_LobbySet("PLAYERS", sizeof(CLIENT.game.players), &CLIENT.game.players);
		np_lobby_set_string("LEVEL", CLIENT.game.level);
	} else if (netmode == NET_JOIN) {
		np_lobby_get_i8(&CLIENT.game.players, "PLAYERS");
		np_lobby_get_string(CLIENT.game.level, "LEVEL");
	}
}

const char* net_error() {
	return last_error;
}

bool is_connected() {
	return netmode != NET_NULL && cur_lobby[0] != '\0';
}

void disconnect() {
	NutPunch_Disconnect();
	SDL_memset(cur_lobby, 0, sizeof(cur_lobby));
	netmode = NET_NULL;
}

// =======
// LOBBIES
// =======

const char* get_lobby_id() {
	return cur_lobby[0] == '\0' ? NULL : cur_lobby;
}

void find_lobby_mode(const char* id) {
	if (id == NULL)
		SDL_memset(cur_lobby, 0, sizeof(cur_lobby));
	else
		SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));

	struct NutPunch_Filter filter = {0};
	SDL_memcpy(filter.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));
	SDL_memcpy(filter.value, &MAGIC_VALUE, sizeof(MAGIC_VALUE));
	filter.comparison = 0;

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

	for (int i = 0; i < NutPunch_LobbyCount(); i++) {
		if (SDL_strcmp(cur_lobby, NutPunch_GetLobby(i)))
			continue;
		if (netmode == NET_HOST) {
			last_error = "Lobby with this name exists";
			return -1;
		} else if (netmode == NET_JOIN) {
			NutPunch_Join(cur_lobby);
			return 1;
		}
	}

	if (netmode == NET_HOST) {
		NutPunch_Host(cur_lobby);
		NutPunch_LobbySet(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
		return 1;
	} else if (netmode == NET_JOIN) {
		last_error = "Lobby doesn't exist";
		return -1;
	}
	return 0;
}

int get_lobby_count() {
	return NutPunch_LobbyCount();
}

const char* get_lobby(int idx) {
	return NutPunch_GetLobby(idx);
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
