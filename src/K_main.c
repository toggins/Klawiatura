#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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

#include "K_cmd.h"
#include "K_game.h"
#include "K_video.h"

static void cmd_ip(IterArg), cmd_level(IterArg);
MAKE_FLAG(bypass_shader);
MAKE_FLAG(skip_intro);
MAKE_FLAG(kevin);

CmdArg CMDLINE[] = {
	{"-s", "-bypass_shader", CMD_FLAG(bypass_shader)},
	{"-i", "-skip_intro",    CMD_FLAG(skip_intro)   },
	{"-K", "-kevin",         CMD_FLAG(kevin)        },
	{"-a", "-ip",            cmd_ip                 },
	{"-l", "-level",         cmd_level              },
	{NULL, NULL,             NULL                   },
};

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

	handle_cmdline(argc, argv);

	if (skip_intro)
		INFO("BYE INTRO!!!");

	if (kevin)
		INFO("HI KEVIN!!!");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
		FATAL("SDL_Init fail: %s", SDL_GetError());
	video_init(bypass_shader);

	load_texture("ui/disclaimer");
	load_texture("enemies/bomzh");

	Surface* dummy = create_surface(128, 128, true, true);

	bool running = true;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event))
			switch (event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				default:
					break;
			}

		clear_color(0, 0, 0, 1);

		push_surface(dummy);
		clear_color(1, 0, 0, 1);
		clear_depth(1);
		batch_sprite("enemies/bomzh", XYZ(32, 192, 0), NO_FLIP, 0, WHITE);
		batch_sprite("enemies/bomzh", XYZ(128, 256, 64), NO_FLIP, 0, WHITE);
		pop_surface();
		batch_surface(dummy, XYZ(32, 32, -16), WHITE);

		batch_sprite("ui/disclaimer", XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0), NO_FLIP, 0, WHITE);

		submit_video();
	}

	destroy_surface(dummy);
	video_teardown();
	SDL_Quit();

	return 0;
}

static void cmd_ip(IterArg next) {
	const char* ip = next();
	if (ip != NULL)
		NutPunch_SetServerAddr(ip);
}

static void cmd_level(IterArg next) {
	const char* level = next();
	if (level != NULL) {
		// TODO: set level...
	}
}
