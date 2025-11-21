#include "K_discord.h"
#include "K_game.h"
#include "K_log.h" // IWYU pragma: keep

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

#endif

void discord_init() {
#ifdef K_DISCORD
	Discord_Client_Init(&discord);
	Discord_Client_SetApplicationId(&discord, DISCORD_APPID);
	Discord_Client_AddLogCallback(&discord, log_callback, NULL, NULL, Discord_LoggingSeverity_Warning);
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

void update_discord_status(const char* level_name) {
#ifdef K_DISCORD
	Discord_Activity activity = {0};
	Discord_Activity_Init(&activity);
	Discord_Activity_SetType(&activity, Discord_ActivityTypes_Playing);
	Discord_Activity_SetName(&activity, DSTR("Klawiatura"));

	char details[32] = "", state[64] = "";

	if (game_exists()) {
		SDL_snprintf(details, sizeof(details), "%s", level_name);
		if (numplayers() > 1L)
			SDL_snprintf(state, sizeof(state), "%u players", numplayers());
	} else if (is_connected()) {
		SDL_snprintf(details, sizeof(details), "Lobby");
		if (in_public_lobby())
			SDL_snprintf(state, sizeof(state), "%s", get_lobby_id());
	} else
		SDL_snprintf(details, sizeof(details), "Menu");

	if (details[0] != '\0')
		Discord_Activity_SetDetails(&activity, &DSTR(details));
	if (state[0] != '\0')
		Discord_Activity_SetState(&activity, &DSTR(state));
	Discord_Client_UpdateRichPresence(&discord, &activity, rpc_callback, NULL, NULL);
	Discord_Activity_Drop(&activity);
#endif
}
