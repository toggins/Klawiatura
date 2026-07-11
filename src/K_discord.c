#ifdef K_DISCORD
#include <cdiscord.h>
#endif

#include "K_cmd.h"
#include "K_discord.h"
#include "K_file.h" // IWYU pragma: export
#include "K_interface.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_worlds.h"

#ifdef K_DISCORD

#define DISCORD_APPID 1441442128445181963

#define STATIC_STRING(str) ((Discord_String){.ptr = (Uint8*)(str), .size = sizeof(str)})
#define DYNAMIC_STRING(str) ((Discord_String){.ptr = (Uint8*)(str), .size = SDL_strlen(str)})

static Discord_Client discord = {0};

static void log_callback(Discord_String dstr, Discord_LoggingSeverity level, void* userdata) {
    (void)userdata;

    switch (level) {
    default:
        INFO("%.*s", (int)dstr.size, dstr.ptr);
        break;
    case Discord_LoggingSeverity_Warning:
        WARN("%.*s", (int)dstr.size, dstr.ptr);
        break;
    case Discord_LoggingSeverity_Error:
        WTF("%.*s", (int)dstr.size, dstr.ptr);
        break;
    }
}

static void rpc_callback(Discord_ClientResult* result, void* userdata) {}

static void invite_callback(Discord_String secret, void* userdata) {
    (void)userdata;

    set_screen(SCR_MENU, secret.ptr, secret.size);
}

#endif

void discord_init() {
#ifdef K_DISCORD
    Discord_Client_Init(&discord);
    Discord_Client_AddLogCallback(&discord, log_callback, NULL, NULL, Discord_LoggingSeverity_Warning);

    Discord_Client_SetApplicationId(&discord, DISCORD_APPID);
    Discord_Client_RegisterLaunchCommand(&discord, DISCORD_APPID, STATIC_STRING(""));
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
    Discord_Client_ClearRichPresence(&discord);
    Discord_Client_Drop(&discord);
#endif
}

void update_discord_status() {
#ifdef K_DISCORD
    Discord_Activity activity = {0};
    Discord_ActivityParty party = {0};
    Discord_ActivitySecrets secrets = {0};
    Discord_ActivityAssets assets = {0};
    Discord_Activity_Init(&activity);
    Discord_ActivityParty_Init(&party);
    Discord_ActivitySecrets_Init(&secrets);
    Discord_ActivityAssets_Init(&assets);

    Discord_Activity_SetType(&activity, Discord_ActivityTypes_Playing);

    if (is_connected()) {
        const NetID lobby_id = get_lobby_id();

        const char* text = fmt("%s %" SDL_PRIu64, CLIENT.server, lobby_id);
        const Uint32 party_id = SDL_crc32(0, text, SDL_strlen(text));

        text = fmt("%X", party_id);
        Discord_ActivityParty_SetId(&party, DYNAMIC_STRING(text));
        Discord_ActivityParty_SetCurrentSize(&party, get_peer_count());
        Discord_ActivityParty_SetMaxSize(&party, get_peer_limit());
        Discord_Activity_SetParty(&activity, &party);

        yyjson_mut_doc* json = yyjson_mut_doc_new(NULL);
        yyjson_mut_val* root = yyjson_mut_obj(json);

        yyjson_mut_doc_set_root(json, root);
        yyjson_mut_obj_add(root, yyjson_mut_strcpy(json, "server"), yyjson_mut_strcpy(json, CLIENT.server));
        yyjson_mut_obj_add(root, yyjson_mut_strcpy(json, "lobby"), yyjson_mut_uint(json, lobby_id));

        size_t len = 0;
        char* buffer = yyjson_mut_write_opts(json, 0, NULL, &len, NULL);
        yyjson_mut_doc_free(json);

        if (buffer != NULL) {
            Discord_ActivitySecrets_SetJoin(&secrets, (Discord_String){.ptr = (Uint8*)buffer, .size = len});
            Discord_Activity_SetSecrets(&activity, &secrets);
            SDL_free(buffer);
        }
    }

    switch (get_screen()) {
    default:
        break;

    case SCR_MENU: {
        Discord_Activity_SetDetails(&activity, is_connected() ? &STATIC_STRING("Lobby") : &STATIC_STRING("Menu"));
        break;
    }

    case SCR_MAP: {
        const World* world = get_world_key(worldcontext()->world);
        if (world == NULL) {
            Discord_Activity_SetDetails(&activity, &STATIC_STRING("In Game"));
            break;
        }

        const char* name = LFMT(fmt("wld_%s", world->name));
        Discord_Activity_SetDetails(&activity, &DYNAMIC_STRING(name));
        break;
    }

    case SCR_GAME: {
        Discord_Activity_SetDetails(&activity, &STATIC_STRING("In Game"));
        break;
    }
    }

    Discord_Client_UpdateRichPresence(&discord, &activity, rpc_callback, NULL, NULL);

    Discord_Activity_Drop(&activity);
    Discord_ActivityParty_Drop(&party);
    Discord_ActivitySecrets_Drop(&secrets);
    Discord_ActivityAssets_Drop(&assets);
#endif
}
