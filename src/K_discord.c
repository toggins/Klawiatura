#include "K_discord.h"
#include "K_game.h"
#include "K_log.h" // IWYU pragma: keep
#include "K_menu.h"

#define DISCORD_APPID 1441442128445181963

#ifdef K_DISCORD

#define DSTR(str) ((Discord_String){(uint8_t*)(str), sizeof(str)})

static Discord_Client discord = {0};

static void log_callback(Discord_String dstr, Discord_LoggingSeverity level, void* userdata) {
	switch (level) {
	default:
		INFO("%s", dstr.ptr);
		break;
	case Discord_LoggingSeverity_Warning:
		WARN("%s", dstr.ptr);
		break;
	case Discord_LoggingSeverity_Error:
		WTF("%s", dstr.ptr);
		break;
	}
}

static void rpc_callback(Discord_ClientResult* result, void* userdata) {}

static void invite_callback(Discord_String secret, void* userdata) {
	nuke_game(), disconnect();
	join_lobby((char*)secret.ptr);
	set_menu(MEN_JOINING_LOBBY);
}

#endif

void discord_init() {
#ifdef K_DISCORD
	Discord_Client_Init(&discord);
	Discord_Client_AddLogCallback(&discord, log_callback, NULL, NULL, Discord_LoggingSeverity_Warning);

	Discord_Client_SetApplicationId(&discord, DISCORD_APPID);
	Discord_Client_RegisterLaunchCommand(&discord, DISCORD_APPID, DSTR("KLAWIATURA://"));
	Discord_Client_SetActivityJoinCallback(&discord, invite_callback, NULL, NULL);
#else
	WARN("Discord RPC is unavailable for this build");
#endif
}

void discord_update() {
#ifdef K_DISCORD
	Discord_RunCallbacks();
#endif
}

void discord_teardown() {
#ifdef K_DISCORD
	Discord_Client_Drop(&discord);
#endif
}

extern GameState game_state;
void update_discord_status(const char* level_name) {
#ifdef K_DISCORD
	Discord_Activity activity = {0};
	Discord_ActivityParty party = {0};
	Discord_ActivitySecrets secrets = {0};
	Discord_Activity_Init(&activity);
	Discord_ActivityParty_Init(&party);
	Discord_ActivitySecrets_Init(&secrets);

	// Information
	Discord_Activity_SetType(&activity, Discord_ActivityTypes_Playing);
	Discord_Activity_SetName(&activity, DSTR("Klawiatura"));

	char details[32] = "", state[32] = "";

	// Game info
	if (game_exists()) {
		SDL_snprintf(details, sizeof(details), "%s", level_name);
		if ((game_state.flags & GF_HELL))
			SDL_snprintf(state, sizeof(state),
				((game_state.flags & GF_HELL) == GF_HELL)
					? "Nostaliga Mode"
					: ((game_state.flags & GF_KEVIN) ? "Kevin Mode" : "Fred Mode"));
	} else
		SDL_snprintf(details, sizeof(details), is_connected() ? "Lobby" : "Menu");

	// Multiplayer info
	if (in_public_server() && is_connected()) {
		Uint32 id = SDL_rand_bits(); // FIXME: Generate a better UUID.
		Discord_ActivityParty_SetId(&party, (Discord_String){(uint8_t*)(&id), sizeof(id)});
		Discord_ActivityParty_SetCurrentSize(&party, get_peer_count());
		Discord_ActivityParty_SetMaxSize(&party, get_max_peers());
		Discord_Activity_SetParty(&activity, &party);

		if (!game_exists()) {
			const char* lobby_id = get_lobby_id();
			if (lobby_id != NULL && !game_exists()) {
				Discord_ActivitySecrets_SetJoin(
					&secrets, (Discord_String){(uint8_t*)lobby_id, SDL_strlen(lobby_id)});
				Discord_Activity_SetSecrets(&activity, &secrets);
			}
		}
	}

	if (details[0] != '\0')
		Discord_Activity_SetDetails(&activity, &DSTR(details));
	if (state[0] != '\0')
		Discord_Activity_SetState(&activity, &DSTR(state));

	Discord_Activity_SetSupportedPlatforms(&activity, Discord_ActivityGamePlatforms_Desktop);
	Discord_Client_UpdateRichPresence(&discord, &activity, rpc_callback, NULL, NULL);

	Discord_Activity_Drop(&activity);
	Discord_ActivityParty_Drop(&party);
	Discord_ActivitySecrets_Drop(&secrets);
#endif
}
