#include <stdio.h> // required for `printf()`

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "K_cmake.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_log.h"
#include "K_os.h"

static const char** mods = NULL;
static size_t num_mods = 0;

static void cmd_mod() {
    const char* path = next_arg();
    if (path == NULL)
        return;

    ++num_mods;
    mods = (const char**)SDL_realloc((void*)mods, num_mods * sizeof(*mods));
    EXPECT(mods, "Failed to reallocate mods list");
    mods[num_mods - 1] = path;
}

MAKE_FLAG(force_shader);

CmdArg CMDLINE[] = {
    {"-m", "-mod",          cmd_mod              },
    {"-s", "-force_shader", CMD_OPT(force_shader)},
    {NULL, NULL,            NULL                 },
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
    file_init(mods, num_mods);

    INFO("Init OK");
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    (void)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    (void)appstate;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    (void)appstate;
    (void)result;

    file_teardown();
    INFO("Quit OK");
    log_teardown();
}
