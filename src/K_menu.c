#include <SDL3/SDL_timer.h>

#include "K_audio.h"
#include "K_game.h"
#include "K_input.h"
#include "K_menu.h"
#include "K_video.h"

static uint64_t last_time = 0;
static float delta_time = 0, ticks = 0, draw_ticks = 0;

static int intro_time = 0;
static MenuType menu = MEN_NULL;

static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {NULL},
	[MEN_INTRO] = {NULL},

	[MEN_MAIN] = {{
		{"Mario Together", NULL, true},
		{NULL, NULL},
		{"Singleplayer", NULL},
		{"Multiplayer", NULL},
		{"Options", NULL},
		{"Exit", NULL},
	}},

	[MEN_SINGLEPLAYER] = {{
		{"Level: FMT", NULL},
		{"Start!", NULL},
	}},

	[MEN_MULTIPLAYER] = {{
		{"Host Lobby", NULL},
		{"Join Lobby", NULL},
		{"Find Lobby", NULL},
	}},

	[MEN_OPTIONS] = {{
		{"Controls", NULL},
		{NULL, NULL, true},
		{"Name: FMT", NULL},
		{"Skin: FMT", NULL},
		{NULL, NULL, true},
		{"Scale: FMT", NULL},
		{"Fullscreen: FMT", NULL},
		{"Vsync: FMT", NULL},
		{NULL, NULL, true},
		{"Master Volume: FMT", NULL},
		{"Sound Volume: FMT", NULL},
		{"Music Volume: FMT", NULL},
	}},

	[MEN_HOST_LOBBY] = {{
		{"Players: FMT", NULL},
		{"Host!", NULL},
	}},

	[MEN_JOIN_LOBBY] = {{
		{"Lobby ID: FMT", NULL},
		{"Join!", NULL},
	}},

	[MEN_FIND_LOBBY] = {NULL},

	[MEN_LOBBY] = {{
		{"Level: FMT", NULL},
		{"Start!", NULL},
	}},
};

void start_menu(bool skip_intro) {
	load_texture("ui/disclaimer");
	load_texture("ui/background");

	load_font("main");
	load_font("hud");

	load_sound("switch");
	load_sound("select");
	load_sound("toggle");
	load_sound("on");
	load_sound("off");
	load_sound("kevin_type");
	load_sound("kevin_on");
	load_sound("thwomp");
	load_sound("connect");
	load_sound("disconnect");

	load_track("title");
	load_track("smw_donut_plains");
	load_track("mk_mario");
	load_track("sf_map");
	load_track("human_like_predator");

	if (skip_intro) {
		set_menu(MEN_MAIN, false);
		play_generic_track("title", true);
	} else {
		set_menu(MEN_INTRO, false);
	}
	last_time = SDL_GetPerformanceCounter();
}

void update_menu() {
	const uint64_t current_time = SDL_GetPerformanceCounter();

	delta_time = ((float)(current_time - last_time) / (float)SDL_GetPerformanceFrequency()) * (float)TICKRATE;
	ticks += delta_time;
	draw_ticks += delta_time;

	last_time = current_time;

	while (ticks >= 1) {
		if (menu == MEN_INTRO) {
			++intro_time;
			if (intro_time >= 150 || kb_pressed(KB_UI_ENTER)) {
				set_menu(MEN_MAIN, false);
				play_generic_track("title", true);
			} else
				goto skip_tick;
		}

		int change = kb_pressed(KB_UI_DOWN) - kb_pressed(KB_UI_UP);
		if (change == 0)
			goto try_select;

		size_t old_option = MENUS[menu].option;
		size_t new_option = old_option;
		bool found_option = false;

		for (size_t i = 0; i < MAX_OPTIONS; i++) {
			if (change < 0 && new_option <= 0)
				new_option = MAX_OPTIONS - 1;
			if (change > 0 && new_option >= (MAX_OPTIONS - 1))
				new_option = 0;
			else
				new_option += change;

			Option* option = &MENUS[menu].options[new_option];
			if (option->name != NULL && !option->disabled) {
				found_option = true;
				break;
			}
		}

		if (found_option && old_option != new_option) {
			MENUS[menu].option = new_option;
			play_generic_sound("switch");
		}

	try_select:
		// TODO

	skip_tick:
		input_newframe();
		--ticks;
	}
}

void draw_menu() {
	start_drawing();
	clear_color(0, 0, 0, 1);

	if (menu == MEN_INTRO) {
		batch_sprite("ui/disclaimer", XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0), NO_FLIP, 0, ALPHA(128));
		goto jobwelldone;
	}

	batch_sprite("ui/background", XYZ(0, 0, 0), NO_FLIP, 0, WHITE);

	MENUS[menu].cursor = glm_lerp(MENUS[menu].cursor, (float)MENUS[menu].option, 0.6f * delta_time);
	batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), ">",
		XYZ(44 + (SDL_sinf(draw_ticks / 5.f) * 4), 24 + (MENUS[menu].cursor * 24), 0), WHITE);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &MENUS[menu].options[i];
		option->hover = glm_lerp(option->hover, (float)(MENUS[menu].option == i), 0.5f * delta_time);
		batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP), option->name,
			XYZ(48 + (option->hover * 8), 24 + (i * 24), 0), (option->disabled) ? ALPHA(128) : WHITE);
	}

jobwelldone:
	stop_drawing();
}

void set_menu(MenuType men, bool allow_return) {
	if (menu == men || men <= MEN_NULL || men >= MEN_SIZE)
		return;

	MENUS[men].from = allow_return ? menu : MEN_NULL;
	menu = men;

	// Go to nearest valid option
	bool found_option = false;
	size_t new_option = MENUS[menu].option;
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &MENUS[menu].options[new_option];
		if (option->name != NULL && !option->disabled) {
			found_option = true;
			break;
		}

		new_option = (new_option + 1) % MAX_OPTIONS;
	}

	if (found_option)
		MENUS[menu].option = new_option;
}
