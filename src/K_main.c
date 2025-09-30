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

#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_file.h"
#include "K_game.h" // IWYU pragma: keep (for now)
#include "K_input.h"
#include "K_menu.h"
#include "K_video.h"

static void cmd_ip(IterArg), cmd_level(IterArg);
MAKE_OPTION(data_path, NULL);
MAKE_OPTION(config_path, NULL);
MAKE_FLAG(force_shader);
MAKE_FLAG(skip_intro);
MAKE_FLAG(kevin);

CmdArg CMDLINE[] = {
	{"-s", "-force_shader", CMD_OPT(force_shader)},
	{"-i", "-skip_intro",   CMD_OPT(skip_intro)  },
	{"-d", "-data",         CMD_OPT(data_path)   },
	{"-c", "-config",       CMD_OPT(config_path) },
	{"-K", "-kevin",        CMD_OPT(kevin)       },
	{"-a", "-ip",           cmd_ip               },
	{"-l", "-level",        cmd_level            },
	{NULL, NULL,            NULL                 },
};

ClientInfo CLIENT = (ClientInfo){
	.user = {"Player", ""},
	.game = {1, false, "test"},
	.lobby = {"Klawiatura", true},
};

bool quickstart = false;
bool permadeath = false;
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

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
		FATAL("SDL_Init fail: %s", SDL_GetError());

	file_init(data_path);
	video_init(force_shader);
	audio_init();
	input_init();
	config_init(config_path);
	net_init();

	start_menu(skip_intro);

	while (!permadeath) {
		SDL_Event event;
		while (SDL_PollEvent(&event))
			switch (event.type) {
			case SDL_EVENT_QUIT:
				goto exit;
			case SDL_EVENT_KEY_DOWN:
				input_keydown(event.key.scancode);
				break;
			case SDL_EVENT_KEY_UP:
				input_keyup(event.key.scancode);
				break;
			case SDL_EVENT_TEXT_INPUT:
				input_text_input(event.text);
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				set_resolution(event.window.data1, event.window.data2);
				break;
			default:
				break;
			}

		if (game_exists()) {
			if (update_game())
				draw_game();
			else
				input_wipeout();
		} else {
			update_menu();
			draw_menu();
		}

		audio_update();
	}

exit:
	nuke_game();
	net_teardown();
	config_teardown();
	input_teardown();
	audio_teardown();
	video_teardown();
	file_teardown();
	SDL_Quit();

	return 0;
}

static void cmd_ip(IterArg next) {
	set_hostname(next());
}

static void cmd_level(IterArg next) {
	SDL_strlcpy(CLIENT.game.level, next(), sizeof(CLIENT.game.level));
	quickstart = true;
}
