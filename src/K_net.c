#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free

#include "K_cmd.h"
#include "K_net.h"

static const char* hostname = NUTPUNCH_DEFAULT_SERVER;

static int status = 0;
static const char* last_error = NULL;

static char cur_lobby[CLIENT_STRING_MAX] = "";
static bool hosting = false;

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

extern ClientInfo CLIENT;
void net_newframe() {
	status = NutPunch_Update();
	last_error = NutPunch_GetLastError();

	if (!is_connected())
		return;

	if (NutPunch_PeerAlive(NutPunch_LocalPeer()))
		NutPunch_PeerSet(
			"NAME", (int)SDL_strnlen(CLIENT.user.name, NUTPUNCH_FIELD_DATA_MAX - 1), CLIENT.user.name);

	if (hosting) {
		NutPunch_LobbySet("PLAYERS", sizeof(CLIENT.game.players), &CLIENT.game.players);
		NutPunch_LobbySet(
			"LEVEL", (int)SDL_strnlen(CLIENT.game.level, NUTPUNCH_FIELD_DATA_MAX - 1), CLIENT.game.level);
	} else {
		int size;

		int8_t* pplayers = (int8_t*)NutPunch_LobbyGet("PLAYERS", &size);
		if (pplayers != NULL && size == sizeof(CLIENT.game.players))
			CLIENT.game.players = *pplayers;

		char* plevel = (char*)NutPunch_LobbyGet("LEVEL", &size);
		if (plevel != NULL && size <= NUTPUNCH_FIELD_DATA_MAX)
			SDL_strlcpy(CLIENT.game.level, plevel, size + 1);
	}
}

const char* net_error() {
	return last_error;
}

bool is_connected() {
	return status == NP_Status_Online && cur_lobby[0] != '\0';
}

void disconnect() {
	NutPunch_Disconnect();
	cur_lobby[0] = '\0';
}

// =======
// LOBBIES
// =======

const char* get_lobby_id() {
	return cur_lobby[0] == '\0' ? NULL : cur_lobby;
}

static void driving_license_check_fr() {
	struct NutPunch_Filter filter = {0};
	SDL_memcpy(filter.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));
	SDL_memcpy(filter.value, &MAGIC_VALUE, sizeof(MAGIC_VALUE));
	filter.comparison = 0;

	NutPunch_FindLobbies(1, &filter);
}

static void driving_license_check(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));
	driving_license_check_fr();
}

void host_lobby(const char* id) {
	driving_license_check(id);
	hosting = true;
}

void join_lobby(const char* id) {
	driving_license_check(id);
	hosting = false;
}

int find_lobby() {
	if (status != NP_Status_Online)
		return -1;

	int n = NutPunch_LobbyCount();
	for (int i = 0; i < n; i++) {
		if (SDL_strcmp(NutPunch_GetLobby(i), cur_lobby) != 0)
			continue;
		if (!hosting) {
			NutPunch_Join(cur_lobby);
			return 1;
		}
		break;
	}
	if (hosting) {
		NutPunch_Host(cur_lobby);
		NutPunch_LobbySet(MAGIC_KEY, sizeof(MAGIC_VALUE), &MAGIC_VALUE);
		return 1;
	}

	return 0;
}

void list_lobbies() {
	driving_license_check_fr();
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
