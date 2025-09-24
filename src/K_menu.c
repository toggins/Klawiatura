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

#define SHORTCUT(fname, men)                                                                                           \
	static void fname() {                                                                                          \
		set_menu(men);                                                                                         \
	}
// Main Menu
SHORTCUT(goto_singleplayer, MEN_SINGLEPLAYER);
SHORTCUT(goto_multiplayer, MEN_MULTIPLAYER);
SHORTCUT(goto_options, MEN_OPTIONS);

extern bool permadeath;
static void instaquit() {
	permadeath = true;
}

// Singleplayer
// TODO

// Multiplayer
SHORTCUT(goto_host_lobby, MEN_HOST_LOBBY);
SHORTCUT(goto_join_lobby, MEN_JOIN_LOBBY);
SHORTCUT(goto_find_lobby, MEN_FIND_LOBBY);

// Host Lobby
// TODO

// Join a Lobby
// TODO

// Find lobbies
// TODO

// Options
SHORTCUT(goto_controls, MEN_CONTROLS);

// Controls
// TODO
#undef SHORTCUT

static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {.noreturn = true},
	[MEN_INTRO] = {.noreturn = true},
	[MEN_MAIN] = {"Mario Together", .noreturn = true},
	[MEN_SINGLEPLAYER] = {"Singleplayer"},
	[MEN_MULTIPLAYER] = {"Multiplayer"},
	[MEN_HOST_LOBBY] = {"Host Lobby"},
	[MEN_JOIN_LOBBY] = {"Join a Lobby"},
	[MEN_FIND_LOBBY] = {"Find Lobbies"},
	[MEN_JOINING_LOBBY] = {"Please wait..."},
	[MEN_OPTIONS] = {"Options"},
	[MEN_CONTROLS] = {"Change Controls"},
};
static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_MAIN] = {
		{"Singleplayer", .callback = goto_singleplayer},
		{"Multiplayer", .callback = goto_multiplayer},
		{"Options", .callback = goto_options},
		{"Exit", .callback = instaquit},
	},
	[MEN_SINGLEPLAYER] = {
		{"Level: FMT"},
		{},
		{"Start!"},
	},
	[MEN_MULTIPLAYER] = {
		{"Host Lobby", .callback = goto_host_lobby},
		{"Join a Lobby", .callback = goto_join_lobby},
		{"Find Lobbies", .callback = goto_find_lobby},
	},
	[MEN_OPTIONS] = {
		{"Change Controls", .callback = goto_controls},
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
	[MEN_CONTROLS] = {
		{"Up: FMT"},
		{"Left: FMT"},
		{"Down: FMT"},
		{"Right: FMT"},
		{},
		{"Jump: FMT"},
		{"Run: FMT"},
		{"Fire: FMT"},
	},
	[MEN_HOST_LOBBY] = {
		// FIXME: Add explicit lobby hosting to NutPunch. (if the lobby ID already exists, add a "(1)" to it or something)
		{"Lobby ID: FMT"},

		// FIXME: Potential features for NutPunch.
		{"Visibility: FMT", .disabled = true},
		{"Password: FMT", .disabled = true},

		{},

		// NOTE: You can have peers beyond MAX_PLAYERS, they'll just become spectators.
		{"Players: FMT"},

		{},
		{"Host!"},
	},
	[MEN_JOIN_LOBBY] = {
		// FIXME: Add explicit lobby joining to NutPunch.
		{"Lobby ID: FMT"},
		{},
		{"Join!"},
	},
	[MEN_FIND_LOBBY] = {
		{"No lobbies found", .disabled = true},
		// FIXME: Display lobbies with player counts
	},
	[MEN_JOINING_LOBBY] = {
		{"Joining lobby \"FMT\"", .disabled = true},
	},
	[MEN_LOBBY] = {
		{"Lobby: FMT", .disabled = true},
		{"Level: FMT"},
		{},
		{"Start!"},
	},
};

static void start_menu_fr() {
	set_menu(MEN_MAIN);
	play_generic_track("title", true);
}

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

	if (skip_intro)
		start_menu_fr();
	else
		set_menu(MEN_INTRO);
}

void update_menu() {
	for (new_frame(); got_ticks(); next_tick()) {
		if (menu == MEN_INTRO) {
			if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER))
				start_menu_fr();
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
		if (kb_pressed(KB_UI_ENTER)) {
			const Option* opt = &OPTIONS[menu][MENUS[menu].option];
			if (opt->callback != NULL && !opt->disabled) {
				play_generic_sound("select");
				opt->callback();
			}
		}

		if (kb_pressed(KB_PAUSE))
			switch (menu) {
				// FIXME: Replace this non-default junk with actual lobby stuff
				case MEN_JOINING_LOBBY: // Should abort connection and return to previous menu
				case MEN_LOBBY: // Should disconnect and return to the menu before MEN_JOINING_LOBBY
					noop();
					break;

				default: {
					if (set_menu(MENUS[menu].from))
						play_generic_sound("select");
					break;
				}
			}
	}
}

void draw_menu() {
	start_drawing();

	if (menu == MEN_INTRO) {
		clear_color(0, 0, 0, 1);

		float a = 1.f;
		const float ticks = totalticks();
		if (ticks < 25)
			a = ticks / 25.f;
		if (ticks > 125)
			a = 1.f - ((ticks - 100.f) / 25.f);

		set_batch_alpha_test(0);
		batch_sprite(
			"ui/disclaimer", XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0), NO_FLIP, 0, ALPHA(a * 255));
		set_batch_alpha_test(0.5f);
		goto jobwelldone;
	}

	batch_sprite("ui/background", XYZ(0, 0, 0), NO_FLIP, 0, WHITE);

	Menu* men = &MENUS[menu];
	const float menu_y = 60;

	const float lx = 0.6f * dt();
	men->cursor = glm_lerp(men->cursor, (float)men->option, SDL_min(lx, 1));
	if (!OPTIONS[menu][men->option].disabled)
		batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), ">",
			XYZ(44 + (SDL_sinf(totalticks() / 5.f) * 4), menu_y + (men->cursor * 24), 0), WHITE);

	batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP), men->name, XYZ(48, 24, 0), RGBA(255, 144, 144, 192));
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[menu][i];

		const float lx = 0.5f * dt();
		option->hover = glm_lerp(option->hover, (float)(men->option == i && !option->disabled), SDL_min(lx, 1));

		batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP),
			option->format != NULL ? option->format() : option->name,
			XYZ(48 + (option->hover * 8), menu_y + (i * 24), 0), option->disabled ? ALPHA(128) : WHITE);
	}

	if (men->from != MEN_NULL) {
		// FIXME: Find a way to display keybinds and replace the hardcoded string here.
		//        KB_PAUSE isn't supposed to be customizable, but I don't care.
		batch_string("main", 24, ALIGN(FA_LEFT, FA_BOTTOM), "[Esc] Back", XYZ(48, SCREEN_HEIGHT - 24, 0),
			ALPHA(128));
	}

jobwelldone:
	stop_drawing();
}

bool set_menu(MenuType men) {
	if (menu == men || men <= MEN_NULL || men >= MEN_SIZE)
		return false;

	if (MENUS[menu].from != men)
		MENUS[men].from = MENUS[men].noreturn ? MEN_NULL : menu;
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

	return true;
}
