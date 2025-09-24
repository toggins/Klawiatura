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
		{"Level: %s"},
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
		{"Name: %s"},
		{"Skin: %s"},
		{},
		{"Scale: %d"},
		{"Fullscreen: %s"},
		{"Vsync: %s"},
		{},
		{"Master Volume: %d"},
		{"Sound Volume: %d"},
		{"Music Volume: %d"},
	},
	[MEN_CONTROLS] = {
		{"Up: %s"},
		{"Left: %s"},
		{"Down: %s"},
		{"Right: %s"},
		{},
		{"Jump: %s"},
		{"Run: %s"},
		{"Fire: %s"},
	},
	[MEN_HOST_LOBBY] = {
		// FIXME: Add explicit lobby hosting to NutPunch. (if the lobby ID already exists, add a "(1)" to it or something)
		{"Lobby ID: %s"},

		// FIXME: Potential features for NutPunch.
		{"Visibility: %s", .disabled = true},
		{"Password: %s", .disabled = true},

		{},

		// NOTE: You can have peers beyond MAX_PLAYERS, they'll just become spectators.
		{"Players: %d"},

		{},
		{"Host!"},
	},
	[MEN_JOIN_LOBBY] = {
		// FIXME: Add explicit lobby joining to NutPunch.
		{"Lobby ID: %s"},
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
		{"Lobby: %s", .disabled = true},
		{"Level: %s"},
		{},
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

		for (size_t i = 0; i < MAX_OPTIONS; i++) {
			if (change < 0 && new_option <= 0)
				new_option = MAX_OPTIONS - 1;
			if (change > 0 && new_option >= (MAX_OPTIONS - 1))
				new_option = 0;
			else
				new_option += change;

			Option* option = &OPTIONS[menu][new_option];
			if (option->name != NULL && !option->disabled)
				goto found_option;
		}
		goto try_select;

	found_option:
		if (old_option != new_option) {
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

		if (!kb_pressed(KB_PAUSE))
			continue; // de-nesting
		// TODO: stuff this into a cleanup function or something?
		switch (menu) {
			// FIXME: Replace this non-default junk with actual lobby stuff
			case MEN_JOINING_LOBBY: // Should abort connection and return to previous menu
			case MEN_LOBBY:         // Should disconnect and return to the menu before MEN_JOINING_LOBBY
				noop();
				break;
			default:
				if (set_menu(MENUS[menu].from))
					play_generic_sound("select");
				break;
		}
	}
}

void draw_menu() {
	start_drawing();

	if (menu == MEN_INTRO)
		goto draw_intro;

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

	goto jobwelldone;

draw_intro:
	clear_color(0, 0, 0, 1);

	float a = 1.f;
	const float ticks = totalticks();
	if (ticks < 25)
		a = ticks / 25.f;
	if (ticks > 125)
		a = 1.f - ((ticks - 100.f) / 25.f);

	set_batch_alpha_test(0);
	batch_sprite("ui/disclaimer", XYZ(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0), NO_FLIP, 0, ALPHA(a * 255));
	set_batch_alpha_test(0.5f);

jobwelldone:
	stop_drawing();
}

bool set_menu(MenuType next_menu) {
	if (menu == next_menu || next_menu <= MEN_NULL || next_menu >= MEN_SIZE)
		return false;

	if (MENUS[menu].from != next_menu)
		MENUS[next_menu].from = MENUS[next_menu].noreturn ? MEN_NULL : menu;
	if ((menu == MEN_NULL || menu == MEN_INTRO) && next_menu == MEN_MAIN)
		play_generic_track("title", true);
	menu = next_menu;

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
