#ifdef K_DISCORD
#include <cdiscord.h>
#endif

#include "K_discord.h"
#include "K_game.h"
#include "K_log.h" // IWYU pragma: keep
#include "K_menu.h"

#ifdef K_DISCORD

#define DISCORD_APPID 1441442128445181963

#define DSTR(str) ((Discord_String){.ptr = (Uint8*)(str), .size = sizeof(str)})

static Discord_Client discord = {0};

static const char* d2cstr(Discord_String dstr) {
	static char buf[256] = "";
	SDL_strlcpy(buf, (char*)dstr.ptr, SDL_min(dstr.size + 1, sizeof(buf)));
	return buf;
}

static void log_callback(Discord_String dstr, Discord_LoggingSeverity level, void* userdata) {
	switch (level) {
	default:
		INFO("%s", d2cstr(dstr));
		break;
	case Discord_LoggingSeverity_Warning:
		WARN("%s", d2cstr(dstr));
		break;
	case Discord_LoggingSeverity_Error:
		WTF("%s", d2cstr(dstr));
		break;
	}
}

static void rpc_callback(Discord_ClientResult* result, void* userdata) {}

static void invite_callback(Discord_String secret, void* userdata) {
	nuke_game(), disconnect();
	join_lobby(d2cstr(secret));
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
		SDL_snprintf(details, sizeof(details), NutPunch_IsReady() ? "Lobby" : "Menu");

	// Multiplayer info
	if (in_public_server() && NutPunch_IsReady()) {
		Uint32 id = get_lobby_party();
		Discord_ActivityParty_SetId(&party, (Discord_String){(Uint8*)(&id), sizeof(id)});
		Discord_ActivityParty_SetCurrentSize(&party, NutPunch_PeerCount());
		Discord_ActivityParty_SetMaxSize(&party, NutPunch_GetMaxPlayers());
		Discord_Activity_SetParty(&activity, &party);

		if (!game_exists()) {
			const char* lobby_id = get_lobby_id();
			if (lobby_id != NULL && !game_exists()) {
				Discord_ActivitySecrets_SetJoin(&secrets,
					(Discord_String){.ptr = (Uint8*)lobby_id, .size = SDL_strlen(lobby_id)});
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
