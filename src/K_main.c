#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define FIX_IMPLEMENTATION
#include <S_fixed.h>

#define StAlloc SDL_malloc
#define StFree SDL_free
#define StMemset SDL_memset
#define StMemcpy SDL_memcpy

#include "K_log.h"
#define StLog FATAL

#define S_TRUCTURES_IMPLEMENTATION
#include <S_tructures.h>

#include "K_video.h"

int main(int argc, char* argv[]) {
    INFO("==========[KLAWIATURA]==========");
    INFO("      MARIO FOREVER ONLINE      ");
    INFO("================================");
    INFO("                                ");
    INFO("         ! DISCLAIMER !         ");
    INFO("   This is a free, open-source  ");
    INFO("project not created for any sort");
    INFO("           of profit.           ");
    INFO(" All assets belong to Nintendo. ");
    INFO("We do not condone any commercial");
    INFO("      use of this project.      ");
    INFO("                                ");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
        FATAL("SDL_Init fail: %s", SDL_GetError());
    video_init(false);

    INFO("Stuff in here");

    video_teardown();
    SDL_Quit();

    return 0;
}
