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
#include "K_config.h"
#include "K_file.h"
#include "K_game.h" // IWYU pragma: keep for now
#include "K_video.h"

static void cmd_ip(IterArg);
MAKE_OPTION(data_path, NULL);
MAKE_OPTION(config_path, NULL);
MAKE_OPTION(level, NULL);
MAKE_FLAG(bypass_shader);
MAKE_FLAG(skip_intro);
MAKE_FLAG(kevin);

CmdArg CMDLINE[] = {
	{"-s", "-bypass_shader", CMD_FLAG(bypass_shader)},
	{"-i", "-skip_intro",    CMD_FLAG(skip_intro)   },
	{"-d", "-data",          CMD_FLAG(data_path)    },
	{"-c", "-config",        CMD_FLAG(config_path)  },
	{"-K", "-kevin",         CMD_FLAG(kevin)        },
	{"-a", "-ip",            cmd_ip                 },
	{"-l", "-level",         CMD_FLAG(level)        },
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
	file_init(data_path);
	video_init(bypass_shader);
	config_init(config_path);

	load_texture("ui/background");
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
				case SDL_EVENT_KEY_DOWN:
					if (event.key.scancode == SDL_SCANCODE_ESCAPE)
						running = false;
					break;
				default:
					break;
			}

		start_drawing();

		clear_color(0, 0, 0, 1);
		batch_sprite("ui/background", XYZ(0, 0, 0), NO_FLIP, 0, WHITE);

		push_surface(dummy);
		clear_color(1, 0, 0, 1);
		clear_depth(1);
		batch_sprite("enemies/bomzh", XYZ(32, 192, 0), NO_FLIP, 0, WHITE);
		batch_sprite("enemies/bomzh", XYZ(128, 256, 64), NO_FLIP, 0, WHITE);
		pop_surface();
		batch_surface(dummy, XYZ(32, 32, 32), WHITE);

		stop_drawing();
	}

	destroy_surface(dummy);

	config_teardown();
	video_teardown();
	file_teardown();
	SDL_Quit();

	return 0;
}

static void cmd_ip(IterArg next) {
	NutPunch_SetServerAddr(next());
}
