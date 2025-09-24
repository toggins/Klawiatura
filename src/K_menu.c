#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_game.h"
#include "K_input.h"
#include "K_menu.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

extern ClientInfo CLIENT;

static MenuType cur_menu = MEN_NULL;

static void noop() {
	// Do nothing as if something was done. For testing purposes only.
}

#define FMT_OPTION(fname, ...)                                                                                         \
	static const char* fmt_##fname(const char* base) {                                                             \
		return fmt(base, __VA_ARGS__);                                                                         \
	}
#define BOOL_OPTION(fname, get, set)                                                                                   \
	static void toggle_##fname(int flip) {                                                                         \
		(set)(!((get)()));                                                                                     \
	}
#define VOLUME_OPTION(vname)                                                                                           \
	static void toggle_##vname(int flip) {                                                                         \
		float volume = get_##vname();                                                                          \
		const float delta = (float)flip * 0.1f;                                                                \
                                                                                                                       \
		if (delta > 0.f) {                                                                                     \
			if (volume < 0.99f && (volume + delta) >= 1.f)                                                 \
				volume = 1.f;                                                                          \
			else if (volume >= 0.99f && (volume + delta) >= 1.f)                                           \
				volume = 0.f;                                                                          \
			else                                                                                           \
				volume += delta;                                                                       \
		} else if (volume > 0.01f && (volume + delta) < 0.f)                                                   \
			volume = 0.f;                                                                                  \
		else if (volume <= 0.01f && (volume + delta) <= 0.f)                                                   \
			volume = 1.f;                                                                                  \
		else                                                                                                   \
			volume += delta;                                                                               \
                                                                                                                       \
		set_##vname(volume);                                                                                   \
	}

// Main Menu
extern bool permadeath;
static void instaquit() {
	permadeath = true;
}

// Singleplayer
FMT_OPTION(level, CLIENT.game.level);

// Multiplayer
// TODO

// Host Lobby
FMT_OPTION(lobby, CLIENT.lobby.name);
FMT_OPTION(lobby_public, CLIENT.lobby.public ? "Public" : "Private");
FMT_OPTION(players, CLIENT.game.players);

static void set_players() {
	++CLIENT.game.players;
	while (CLIENT.game.players > MAX_PLAYERS)
		CLIENT.game.players -= MAX_PLAYERS;
}

// Join a Lobby
// TODO

// Find lobbies
// TODO

// Options
FMT_OPTION(name, CLIENT.user.name);
FMT_OPTION(skin, CLIENT.user.skin);

static const char* fmt_resolution(const char* base) {
	int width, height;
	get_resolution(&width, &height);

	return fmt(base, width, height);
}

static void toggle_resolution(int flip) {
	int width, height;
	get_resolution(&width, &height);

	const float sx = (float)width / (float)SCREEN_WIDTH;
	const float sy = (float)height / (float)SCREEN_HEIGHT;
	const float scale = SDL_max(sx, sy);

	if (flip >= 0) {
		if (scale < 1.25)
			set_resolution(SCREEN_WIDTH * 1.5, SCREEN_HEIGHT * 1.5);
		else if (scale >= 1.25 && scale < 1.75)
			set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2);
		else if (scale >= 1.75)
			set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT);
	} else if (scale < 1.25)
		set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2);
	else if (scale >= 1.25 && scale < 1.75)
		set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT);
	else if (scale >= 1.75)
		set_resolution(SCREEN_WIDTH * 1.5, SCREEN_HEIGHT * 1.5);
}

FMT_OPTION(fullscreen, get_fullscreen() ? "On" : "Off");
BOOL_OPTION(fullscreen, get_fullscreen, set_fullscreen);

FMT_OPTION(vsync, get_vsync() ? "On" : "Off");
BOOL_OPTION(vsync, get_vsync, set_vsync);

FMT_OPTION(volume, get_volume() * 100);
VOLUME_OPTION(volume);

FMT_OPTION(sound_volume, get_sound_volume() * 100);
VOLUME_OPTION(sound_volume);

FMT_OPTION(music_volume, get_music_volume() * 100);
VOLUME_OPTION(music_volume);

// Controls
FMT_OPTION(up, kb_label(KB_UP));
FMT_OPTION(left, kb_label(KB_LEFT));
FMT_OPTION(down, kb_label(KB_DOWN));
FMT_OPTION(right, kb_label(KB_RIGHT));
FMT_OPTION(jump, kb_label(KB_JUMP));
FMT_OPTION(fire, kb_label(KB_FIRE));
FMT_OPTION(run, kb_label(KB_RUN));

#undef FMT_OPTION
#undef BOOL_OPTION
#undef VOLUME_OPTION

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

#define EDIT(var) .edit = (var), .edit_size = sizeof(var)
static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_MAIN] = {
		{"Singleplayer", .enter = MEN_SINGLEPLAYER},
		{"Multiplayer", .enter = MEN_MULTIPLAYER},
		{"Options", .enter = MEN_OPTIONS},
		{"Exit", .callback = instaquit},
	},
	[MEN_SINGLEPLAYER] = {
		{"Level: %s", .format = fmt_level, EDIT(CLIENT.game.level)},
		{},
		{"Start!"},
	},
	[MEN_MULTIPLAYER] = {
		{"Host Lobby", .enter = MEN_HOST_LOBBY},
		{"Join a Lobby", .enter = MEN_JOIN_LOBBY},
		{"Find Lobbies", .enter = MEN_FIND_LOBBY},
	},
	[MEN_OPTIONS] = {
		{"Change Controls", .enter = MEN_CONTROLS},
		{},
		{"Name: %s", .format = fmt_name, EDIT(CLIENT.user.name)},
		{"Skin: %s", .format = fmt_skin, EDIT(CLIENT.user.skin)},
		{},
		{"Resolution: %dx%d", .format = fmt_resolution, .callback = toggle_resolution},
		{"Fullscreen: %s", .format = fmt_fullscreen, .callback = toggle_fullscreen},
		{"Vsync: %s", .format = fmt_vsync, .callback = toggle_vsync},
		{},
		{"Master Volume: %.0f%%", .format = fmt_volume, .callback = toggle_volume},
		{"Sound Volume: %.0f%%", .format = fmt_sound_volume, .callback = toggle_sound_volume},
		{"Music Volume: %.0f%%", .format = fmt_music_volume, .callback = toggle_music_volume},
	},
	[MEN_CONTROLS] = {
		{"Up: %s", .format = fmt_up},
		{"Left: %s", .format = fmt_left},
		{"Down: %s", .format = fmt_down},
		{"Right: %s", .format = fmt_right},
		{},
		{"Jump: %s", .format = fmt_jump},
		{"Run: %s", .format = fmt_run},
		{"Fire: %s", .format = fmt_fire},
	},
	[MEN_HOST_LOBBY] = {
		// FIXME: Add explicit lobby hosting to NutPunch. (if the lobby ID already exists, add a "(1)" to it or something)
		{"Lobby ID: %s", .format = fmt_lobby, EDIT(CLIENT.lobby.name)},

		// FIXME: Potential feature for NutPunch.
		{"Visibility: %s", .disabled = true, .format = fmt_lobby_public},

		{},
		{"Host!"},
	},
	[MEN_JOIN_LOBBY] = {
		// FIXME: Add explicit lobby joining to NutPunch.
		{"Lobby ID: %s", .format = fmt_lobby, EDIT(CLIENT.lobby.name)},
		{},
		{"Join!"},
	},
	[MEN_FIND_LOBBY] = {
		{"No lobbies found", .disabled = true},
		// FIXME: Display lobbies with player counts
	},
	[MEN_JOINING_LOBBY] = {
		{"Joining lobby \"%s\"", .disabled = true, .format = fmt_lobby},
	},
	[MEN_LOBBY] = {
		{"Lobby: %s", .disabled = true, .format = fmt_lobby},
		{},
		{"Players: %d", .format = fmt_players, .callback = set_players},
		{"Level: %s", .format = fmt_level, EDIT(CLIENT.game.level)},
		{},
		{"Start!"},
	},
};
#undef EDIT

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
		if (cur_menu == MEN_INTRO) {
			if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER))
				set_menu(MEN_MAIN);
			continue;
		}

		// GROSS HACK: Disable "Resolution" option on fullscreen
		OPTIONS[MEN_OPTIONS][5].disabled = get_fullscreen();

		int change = kb_repeated(KB_UI_DOWN) - kb_repeated(KB_UI_UP);
		if (!change)
			goto try_select;

		size_t old_option = MENUS[cur_menu].option;
		size_t new_option = old_option;

		for (size_t i = 0; i < MAX_OPTIONS; i++) {
			if (change < 0 && new_option <= 0)
				new_option = MAX_OPTIONS - 1;
			if (change > 0 && new_option >= (MAX_OPTIONS - 1))
				new_option = 0;
			else
				new_option += change;

			Option* option = &OPTIONS[cur_menu][new_option];
			if (option->name != NULL && !option->disabled)
				goto found_option;
		}
		goto try_select;

	found_option:
		if (old_option != new_option) {
			MENUS[cur_menu].option = new_option;
			play_generic_sound("switch");
		}

	try_select:
		bool entered = kb_pressed(KB_UI_ENTER);
		int flip = kb_repeated(KB_UI_RIGHT) - kb_repeated(KB_UI_LEFT);
		if (flip != 0 || entered) {
			const Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
			if (opt->callback != NULL && !opt->disabled) {
				play_generic_sound("select");
				opt->callback(flip == 0 ? 1 : flip);
			}
			if (opt->edit != NULL && entered) {
				play_generic_sound("select");
				start_typing(opt->edit, opt->edit_size); // FIXME: Play "select" when pressing Enter or
				                                         //        backspace while typing
			}
			if (opt->enter != MEN_NULL && entered) {
				play_generic_sound("select");
				set_menu(opt->enter);
			}
		}

		if (!kb_pressed(KB_PAUSE))
			continue; // de-nesting
		// TODO: stuff this into a cleanup function or something?
		switch (cur_menu) {
			// FIXME: Replace this non-default junk with actual lobby stuff
			case MEN_JOINING_LOBBY: // Should abort connection and return to previous menu
			case MEN_LOBBY:         // Should disconnect and return to the menu before MEN_JOINING_LOBBY
				noop();
				break;
			default:
				if (set_menu(MENUS[cur_menu].from))
					play_generic_sound("select");
				break;
		}
	}
}

void draw_menu() {
	start_drawing();

	if (cur_menu == MEN_INTRO)
		goto draw_intro;

	batch_cursor(0.f, 0.f, 0.f);
	batch_color(WHITE);
	batch_sprite("ui/background", NO_FLIP);

	Menu* menu = &MENUS[cur_menu];
	const float menu_y = 60;

	const float lx = 0.6f * dt();
	menu->cursor = glm_lerp(menu->cursor, (float)menu->option, SDL_min(lx, 1));
	if (!OPTIONS[cur_menu][menu->option].disabled) {
		batch_cursor(44 + (SDL_sinf(totalticks() / 5.f) * 4), menu_y + (menu->cursor * 24), 0);
		batch_color(WHITE);
		batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), ">");
	}

	batch_cursor(48, 24, 0);
	batch_color(RGBA(255, 144, 144, 192));
	batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP), menu->name);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[cur_menu][i];
		if (opt->name == NULL)
			continue;

		const float lx = 0.5f * dt();
		opt->hover = glm_lerp(opt->hover, (float)(menu->option == i && !opt->disabled), SDL_min(lx, 1));

		const char *suffix = "", *format = opt->format == NULL ? opt->name : opt->format(opt->name);
		if (opt->edit != NULL && typing_what() == opt->edit && SDL_fmodf(totalticks(), 30) < 16)
			suffix = "|";
		const char* name = fmt("%s%s", format, suffix);

		batch_cursor(48.f + (opt->hover * 8.f), menu_y + ((GLfloat)i * 24.f), 0.f);
		batch_color(opt->disabled ? ALPHA(128) : WHITE);
		batch_string("main", 24, ALIGN(FA_LEFT, FA_TOP), name);
	}

	if (menu->from != MEN_NULL) {
		const char *btn = (cur_menu == MEN_JOINING_LOBBY || cur_menu == MEN_LOBBY) ? "Disconnect" : "Back",
			   *indicator = fmt("[%s] %s", kb_label(KB_PAUSE), btn);
		batch_cursor(48, SCREEN_HEIGHT - 24, 0);
		batch_color(ALPHA(128));
		batch_string("main", 24, ALIGN(FA_LEFT, FA_BOTTOM), indicator);
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

	batch_alpha_test(0);
	batch_cursor(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT, 0.f);
	batch_color(ALPHA(a * 255));
	batch_sprite("ui/disclaimer", NO_FLIP);
	batch_alpha_test(0.5f);

jobwelldone:
	stop_drawing();
}

bool set_menu(MenuType next_menu) {
	if (cur_menu == next_menu || next_menu <= MEN_NULL || next_menu >= MEN_SIZE)
		return false;

	if (MENUS[cur_menu].from != next_menu)
		MENUS[next_menu].from = MENUS[next_menu].noreturn ? MEN_NULL : cur_menu;
	switch (cur_menu) {
		case MEN_NULL:
		case MEN_INTRO:
			if (next_menu == MEN_MAIN)
				play_generic_track("title", PLAY_LOOPING);
			break;

		case MEN_OPTIONS:
		case MEN_CONTROLS:
			if (next_menu != MEN_OPTIONS && next_menu != MEN_CONTROLS)
				save_config();
			break;

		default:
			break;
	}
	cur_menu = next_menu;

	// Go to nearest valid option
	size_t new_option = MENUS[cur_menu].option;
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[cur_menu][new_option];
		if (option->name != NULL && !option->disabled) {
			MENUS[cur_menu].option = new_option;
			MENUS[cur_menu].cursor = (float)new_option;
			option->hover = 1;
			break;
		}
		new_option = (new_option + 1) % MAX_OPTIONS;
	}

	return true;
}
