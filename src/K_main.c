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
#include "K_cmd.h"
#include "K_config.h"
#include "K_discord.h"
#include "K_file.h"
#include "K_input.h"
#include "K_menu.h"
#include "K_os.h"
#include "K_video.h"

static void cmd_skip_intro(), cmd_ip(), cmd_level(), cmd_kevin(), cmd_fred(), cmd_string(), cmd_host(), cmd_join();
MAKE_OPTION(data_path, NULL);
MAKE_OPTION(config_path, NULL);
MAKE_FLAG(force_shader);

CmdArg CMDLINE[] = {
	{"-s", "-force_shader", CMD_OPT(force_shader)},
	{"-i", "-skip_intro",   cmd_skip_intro       },
	{"-d", "-data",         CMD_OPT(data_path)   },
	{"-c", "-config",       CMD_OPT(config_path) },
	{"-K", "-kevin",        cmd_kevin            },
	{"-F", "-fred",         cmd_fred             },
	{"-a", "-ip",           cmd_ip               },
	{"-l", "-level",        cmd_level            },
	{"-S", "-string",       cmd_string           },
	{"-h", "-host",         cmd_host             },
	{"-j", "-join",         cmd_join             },
	{NULL, NULL,            NULL                 },
};

ClientInfo CLIENT = {
	.user.name = "", // filled in later in `K_config.c`
	.user.skin = "",
	.input.delay = 2,
	.game.players = 2,
	.game.level = "test",
	.lobby.name = "", // filled in inside `K_menu.c`
	.lobby.public = true,
};

bool quickstart = false, permadeath = false;
static MenuType starting_menu = MEN_INTRO;
static int realmain();

static void show_disclaimer() {
	printf("==========[KLAWIATURA]==========\n");
	printf("      MARIO FOREVER ONLINE      \n");
	printf("================================\n");
	printf("                                \n");
	printf("         ! DISCLAIMER !         \n");
	printf("   This is a free, open-source  \n");
	printf("project not created for any sort\n");
	printf("           of profit.           \n");
	printf(" All assets belong to Nintendo. \n");
	printf("We do not condone any commercial\n");
	printf("      use of this project.      \n");
	printf("                                \n");
}

int main(int argc, char* argv[]) {
	fix_stdio(), show_disclaimer();
	handle_cmdline(argc, argv);
	return realmain();
}

static int realmain() {
	extern void log_init();
	log_init();

	EXPECT(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS), "SDL_Init fail: %s", SDL_GetError());

	extern void populate_actors_table();
	populate_actors_table();

	file_init(data_path), video_init(force_shader), audio_init();
	input_init(), config_init(config_path), discord_init(), net_init(), menu_init();

	if (quickstart) {
		GameContext ctx;
		setup_game_context(&ctx, CLIENT.game.level, GF_SINGLE | GF_TRY_HELL);
		start_game(&ctx);
	} else {
		switch (starting_menu) {
		default:
			break;

		case MEN_HOST_LOBBY: {
			set_menu(MEN_MAIN), set_menu(MEN_MULTIPLAYER), set_menu(MEN_HOST_LOBBY);
			host_lobby(CLIENT.lobby.name);
			starting_menu = MEN_JOINING_LOBBY;
			break;
		}

		case MEN_JOIN_LOBBY: {
			set_menu(MEN_MAIN), set_menu(MEN_MULTIPLAYER), set_menu(MEN_JOIN_LOBBY);
			join_lobby(CLIENT.lobby.name);
			starting_menu = MEN_JOINING_LOBBY;
			break;
		}
		}
		set_menu(starting_menu);
	}

	while (!permadeath) {
		SDL_Event event;
		while (SDL_PollEvent(&event))
			switch (event.type) {
			case SDL_EVENT_QUIT:
				goto teardown;
			case SDL_EVENT_KEY_DOWN:
				input_keydown(event.key);
				break;
			case SDL_EVENT_KEY_UP:
				input_keyup(event.key);
				break;
			case SDL_EVENT_GAMEPAD_ADDED:
				input_gamepadon(event.gdevice);
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				input_gamepadoff(event.gdevice);
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
				input_buttondown(event.gbutton);
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
				input_buttonup(event.gbutton);
				break;
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
				input_axis(event.gaxis);
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
		discord_update();
	}

teardown:
	nuke_game(), net_teardown(), discord_teardown(), config_teardown(), input_teardown();
	audio_teardown(), video_teardown(), file_teardown();
	SDL_Quit();

	return EXIT_SUCCESS;
}

static void cmd_skip_intro() {
	if (starting_menu == MEN_INTRO)
		starting_menu = MEN_MAIN;
}

static void cmd_ip() {
	set_hostname(next_arg());
}

static void cmd_level() {
	SDL_strlcpy(CLIENT.game.level, next_arg(), sizeof(CLIENT.game.level));
	if (!quickstart) {
		quickstart = true;
		INFO("Running in quickstart mode");
	}
}

static void cmd_kevin() {
	if (!CLIENT.game.kevin) {
		CLIENT.game.kevin = true;
		INFO("Kevin mode activated. Good luck...");
	}
}

static void cmd_fred() {
	if (!CLIENT.game.fred) {
		CLIENT.game.fred = true;
		INFO("Fred mode activated. Good luck...");
	}
}

static void cmd_string() {
	int8_t in[ACTOR_STRING_MAX + 1] = "";
	GameActor out = {0};

	SDL_strlcpy((char*)in, next_arg(), sizeof(in));
	encode_actor_string(&out, 0, in);

	FATAL("String \"%s\" encoded to [%i,%i,%i,%i]", in, out.values[0], out.values[1], out.values[2], out.values[3]);
}

static void cmd_host() {
	SDL_strlcpy(CLIENT.lobby.name, next_arg(), sizeof(CLIENT.lobby.name));
	CLIENT.lobby.public = (bool)SDL_strtoul(next_arg(), NULL, 10);
	CLIENT.game.players = (PlayerID)SDL_strtoul(next_arg(), NULL, 10);
	SDL_clamp(CLIENT.game.players, 2, MAX_PLAYERS);
	SDL_strlcpy(CLIENT.game.level, next_arg(), sizeof(CLIENT.game.level));
	starting_menu = MEN_HOST_LOBBY;
}

static void cmd_join() {
	SDL_strlcpy(CLIENT.lobby.name, next_arg(), sizeof(CLIENT.lobby.name));
	starting_menu = MEN_JOIN_LOBBY;
}
