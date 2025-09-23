#include "K_audio.h"
#include "K_game.h"
#include "K_input.h"
#include "K_menu.h"
#include "K_tick.h"
#include "K_video.h"

static MenuType menu = MEN_NULL;

static void noop() {
	// Do nothing as if something was done. For testing purposes only.
}

extern bool permadeath;
static void instaquit() {
	permadeath = true;
}

static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {.noreturn = true},
	[MEN_INTRO] = {.noreturn = true},
	[MEN_MAIN] = {.noreturn = true},
};
static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_MAIN] = {
		{"Mario Together", .disabled = true},
		{},
		{"Singleplayer", .callback = noop}, // just to hear select.wav
		{"Multiplayer"},
		{"Options"},
		{"Exit", .callback = instaquit},
	},
	[MEN_SINGLEPLAYER] = {
		{"Level: FMT"},
		{"Start!"},
	},
	[MEN_MULTIPLAYER] = {
		{"Host Lobby"},
		{"Join Lobby"},
		{"Find Lobby"},
	},
	[MEN_OPTIONS] = {
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
	},
	[MEN_HOST_LOBBY] = {
		{"Players: FMT"},
		{"Host!"},
	},
	[MEN_JOIN_LOBBY] = {
		{"Lobby ID: FMT"},
		{"Join!"},
	},
	[MEN_FIND_LOBBY] = {},
	[MEN_LOBBY] = {
		{"Level: FMT"},
		{"Start!"},
	},
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

	set_menu(skip_intro ? MEN_MAIN : MEN_INTRO);
}

void update_menu() {
	for (new_frame(); got_ticks(); next_tick()) {
		if (menu == MEN_INTRO) {
			if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER))
				set_menu(MEN_MAIN);
			continue;
		}

		int change = kb_repeated(KB_UI_DOWN) - kb_repeated(KB_UI_UP);
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

			Option* option = &OPTIONS[menu][new_option];
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
		const Option* opt = &OPTIONS[menu][MENUS[menu].option];
		if (kb_pressed(KB_UI_ENTER) && opt->callback != NULL && !opt->disabled) {
			play_generic_sound("select");
			opt->callback();
		}
	}
}

void draw_menu() {
	start_drawing();

	if (menu == MEN_INTRO) {
		clear_color(0, 0, 0, 1);
		set_batch_alpha_test(0);
		batch_sprite("ui/disclaimer", XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0), NO_FLIP, 0, WHITE);
		set_batch_alpha_test(0.5f);
		goto jobwelldone;
	}

	batch_sprite("ui/background", XYZ(0, 0, 0), NO_FLIP, 0, WHITE);

	const float lx = 0.6f * dt();
	MENUS[menu].cursor = glm_lerp(MENUS[menu].cursor, (float)MENUS[menu].option, SDL_min(lx, 1));
	batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), ">",
		XYZ(44 + (SDL_sinf(totalticks() / 5.f) * 4), 24 + (MENUS[menu].cursor * 24), 0), WHITE);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[menu][i];

		const float lx = 0.5f * dt();
		option->hover = glm_lerp(option->hover, (float)(MENUS[menu].option == i), SDL_min(lx, 1));

		batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP), option->name,
			XYZ(48 + (option->hover * 8), 24 + (i * 24), 0), (option->disabled) ? ALPHA(128) : WHITE);
	}

jobwelldone:
	stop_drawing();
}

void set_menu(MenuType men) {
	if (menu == men || men <= MEN_NULL || men >= MEN_SIZE)
		return;

	MENUS[men].from = MENUS[men].noreturn ? menu : MEN_NULL;
	menu = men;

	// Go to nearest valid option
	size_t new_option = MENUS[menu].option;
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[menu][new_option];
		if (option->name != NULL && !option->disabled) {
			MENUS[menu].option = new_option;
			MENUS[menu].cursor = (float)new_option;
			option->hover = 1;
			break;
		}
		new_option = (new_option + 1) % MAX_OPTIONS;
	}

	if (menu == MEN_MAIN)
		play_generic_track("title", true);
}
