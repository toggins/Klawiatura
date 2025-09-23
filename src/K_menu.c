#include "K_menu.h"
#include "K_audio.h"
#include "K_game.h"
#include "K_input.h"
#include "K_state.h"
#include "K_video.h"

static MenuType menu = MEN_NULL;

extern bool permadeath;
static void instaquit() {
	permadeath = true;
}

static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {NULL},
	[MEN_INTRO] = {NULL},

	[MEN_MAIN] = {{
		{"Mario Together", .disabled = true},
		{},
		{"Singleplayer"},
		{"Multiplayer"},
		{"Options"},
		{"Exit", .callback = instaquit},
	}},

	[MEN_SINGLEPLAYER] = {{
		{"Level: FMT"},
		{"Start!"},
	}},

	[MEN_MULTIPLAYER] = {{
		{"Host Lobby"},
		{"Join Lobby"},
		{"Find Lobby"},
	}},

	[MEN_OPTIONS] = {{
		{"Controls"},
		{},
		{"Name: FMT"},
		{"Skin: FMT"},
		{},
		{"Scale: FMT"},
		{"Fullscreen: FMT"},
		{"Vsync: FMT"},
		{},
		{"Master Volume: FMT"},
		{"Sound Volume: FMT"},
		{"Music Volume: FMT"},
	}},

	[MEN_HOST_LOBBY] = {{
		{"Players: FMT"},
		{"Host!"},
	}},

	[MEN_JOIN_LOBBY] = {{
		{"Lobby ID: FMT"},
		{"Join!"},
	}},

	[MEN_FIND_LOBBY] = {{}},

	[MEN_LOBBY] = {{
		{"Level: FMT"},
		{"Start!"},
	}},
};

void start_menu(bool skip_intro) {
	from_scratch();

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
	} else
		set_menu(MEN_INTRO, false);
}

void update_menu() {
	for (new_frame(); got_ticks(); next_tick()) {
		if (menu == MEN_INTRO) {
			if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER)) {
				set_menu(MEN_MAIN, false);
				play_generic_track("title", true);
			} else
				goto skip_tick;
		}

		int change = kb_pressed(KB_UI_DOWN) - kb_pressed(KB_UI_UP);
		if (!change)
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

	MENUS[menu].cursor = glm_lerp(MENUS[menu].cursor, (float)MENUS[menu].option, 0.6f * dt());
	batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), ">",
		XYZ(44 + (SDL_sinf(totalticks() / 5.f) * 4), 24 + (MENUS[menu].cursor * 24), 0), WHITE);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &MENUS[menu].options[i];
		option->hover = glm_lerp(option->hover, (float)(MENUS[menu].option == i), 0.5f * dt());
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
	size_t new_option = MENUS[menu].option;
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &MENUS[menu].options[new_option];
		if (option->name != NULL && !option->disabled) {
			MENUS[menu].option = new_option;
			break;
		}
		new_option = (new_option + 1) % MAX_OPTIONS;
	}
}
