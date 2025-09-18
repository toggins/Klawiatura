#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "K_log.h"

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

    INFO("Stuff in here");

    SDL_Quit();

    return 0;
}
