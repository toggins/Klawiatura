#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "K_log.h"

#define S_TRUCTURES_IMPLEMENTATION
#define S_TRUCTURES_NOSTD

#define StAlloc SDL_malloc
#define StFree SDL_free
#define StMemset SDL_memset
#define StMemcpy SDL_memcpy
#define StLog WARN
#define StDie() handle_fatal(__FILE__, __LINE__, __func__, "Out of memory!!!")

#include <S_tructures.h>

#define FIX_IMPLEMENTATION
#include <S_fixed.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_cmake.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_discord.h"
#include "K_file.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_os.h"
#include "K_video.h"
#include "K_worlds.h"

static const char** mods = NULL;

static void cmd_mod() {
    const char* path = next_arg();
    if (path == NULL)
        return;

    if (mods == NULL)
        mods = (const char**)MakeTinyDPro(sizeof(mods), sizeof(*mods));
    mods = (const char**)TinyDPush((void*)mods, (void*)&path);
}

MAKE_FLAG(force_shader);

CmdArg CMDLINE[] = {
    {"-m", "-mod",          cmd_mod              },
    {"-s", "-force_shader", CMD_OPT(force_shader)},
    {NULL, NULL,            NULL                 },
};

ClientInfo CLIENT = {
    .name = DEFAULT_NAME,
    .language = DEFAULT_LANGUAGE,
    .show_user_messages = TRUE,
    .server = "",
    .lobby_limit = MAX_PEERS,
    .input_delay = 2,
    .texture_filter = TRUE,
    .show_hud = TRUE,
};

static void show_disclaimer() {
    printf("\n");
    printf("[" GAME_NAME " " GAME_VERSION "]\n");
    printf("DISCLAIMER:\n");
    printf("This is a free, open-source project not created for any sort of profit.\n");
    printf("We do not condone any commercial use of this project.\n");
    printf("Mario and related characters (c) Nintendo.\n");
    printf("\n");
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    (void)appstate;

    fix_stdio();
    show_disclaimer();
    handle_cmdline(argc, argv);

    log_init();
    EXPECT(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS),
        "Failed to initialize SDL: %s", SDL_GetError());
    file_init(mods);
    video_init(force_shader);
    audio_init();
    locale_init();
    input_init();
    config_init();
    discord_init();
    interface_init();
    game_init();
    worlds_init();
    levels_init();
    chat_init();
    net_init();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    (void)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_WINDOW_RESIZED:
        set_resolution(event->window.data1, event->window.data2, FALSE);
        break;
    case SDL_EVENT_KEY_DOWN:
        input_keydown(event->key);
        break;
    case SDL_EVENT_KEY_UP:
        input_keyup(event->key);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        input_gamepadon(event->gdevice);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        input_gamepadoff(event->gdevice);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        input_buttondown(event->gbutton);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        input_buttonup(event->gbutton);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        input_axis(event->gaxis);
        break;
    case SDL_EVENT_TEXT_INPUT:
        input_text_input(event->text);
        break;
    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    (void)appstate;

    net_update();
    interface_update();
    audio_update();
    discord_update();

    limit_framerate();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    (void)appstate;
    (void)result;

    net_teardown();
    chat_teardown();
    levels_teardown();
    worlds_teardown();
    interface_teardown();
    discord_teardown();
    config_teardown();
    input_teardown();
    locale_teardown();
    audio_teardown();
    video_teardown();
    file_teardown();
    log_teardown();
}
