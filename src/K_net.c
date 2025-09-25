#define NUTPUNCH_IMPLEMENTATION
#define NutPunch_Memcmp SDL_memcmp
#define NutPunch_Memset SDL_memset
#define NutPunch_Memcpy SDL_memcpy
#define NutPunch_Malloc SDL_malloc
#define NutPunch_Free SDL_free

#include "K_cmd.h"
#include "K_log.h"
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

void net_newframe() {
	status = NutPunch_Update();
	last_error = NutPunch_GetLastError();
}

const char* net_error() {
	return last_error;
}

bool is_connected() {
	return status == NP_Status_Online;
}

// =======
// LOBBIES
// =======

const char* get_lobby_id() {
	return cur_lobby;
}

static void driving_license_check(const char* id) {
	SDL_strlcpy(cur_lobby, id, sizeof(cur_lobby));

	struct NutPunch_Filter filter = {0};
	SDL_memcpy(filter.name, MAGIC_KEY, SDL_strnlen(MAGIC_KEY, NUTPUNCH_FIELD_NAME_MAX));
	SDL_memcpy(filter.value, &MAGIC_VALUE, sizeof(MAGIC_VALUE));
	filter.comparison = 0;

	NutPunch_FindLobbies(1, &filter);
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

void disconnect() {
	NutPunch_Disconnect();
}
