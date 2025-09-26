#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_game.h"
#include "K_input.h"
#include "K_log.h"
#include "K_menu.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

extern ClientInfo CLIENT;

static MenuType cur_menu = MEN_NULL;

static float volume_toggle_impl(float, int);
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
		set_##vname(volume_toggle_impl(get_##vname(), flip));                                                  \
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

static void do_host_fr() {
	host_lobby(CLIENT.lobby.name);
	set_menu(MEN_JOINING_LOBBY);
}

// Join a Lobby
static void do_join_fr() {
	join_lobby(CLIENT.lobby.name);
	set_menu(MEN_JOINING_LOBBY);
}

// Lobby
FMT_OPTION(active_lobby, get_lobby_id());

static void set_players(int flip) {
	if (flip >= 0)
		CLIENT.game.players = (int8_t)(CLIENT.game.players >= MAX_PLAYERS ? 0 : (CLIENT.game.players + 1));
	else
		CLIENT.game.players = (int8_t)(CLIENT.game.players <= 1 ? MAX_PLAYERS : (CLIENT.game.players - 1));
}

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

static void update_intro(), update_find_lobbies(), update_joining_lobby();
static void maybe_save_config(MenuType), cleanup_lobby_list(MenuType), maybe_play_title(MenuType);

static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {.noreturn = true},
	[MEN_INTRO] = {.noreturn = true, .update = update_intro},
	[MEN_MAIN] = {"Mario Together", .noreturn = true},
	[MEN_SINGLEPLAYER] = {"Singleplayer"},
	[MEN_MULTIPLAYER] = {"Multiplayer"},
	[MEN_HOST_LOBBY] = {"Host Lobby"},
	[MEN_JOIN_LOBBY] = {"Join a Lobby"},
	[MEN_FIND_LOBBY]
	= {"Find Lobbies", .update = update_find_lobbies, .enter = list_lobbies, .leave = cleanup_lobby_list},
	[MEN_JOINING_LOBBY] = {"Please wait...", .update = update_joining_lobby, .back_sound = "disconnect",
		      .leave = disconnect, .ghost = true},
	[MEN_LOBBY] = {"Lobby", .back_sound = "disconnect", .leave = disconnect},
	[MEN_OPTIONS] = {"Options", .leave = maybe_save_config},
	[MEN_CONTROLS] = {"Change Controls", .leave = maybe_save_config},
};

// Find lobbies
static void join_found_lobby() {
	const char* lober = get_lobby((int)MENUS[MEN_FIND_LOBBY].option);
	if (lober != NULL) {
		join_lobby(lober);
		set_menu(MEN_JOINING_LOBBY);
	}
}

#define EDIT(var) .edit = (var), .edit_size = sizeof(var)
static const char* NO_LOBBIES_FOUND = "No lobbies found";
static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_MAIN] = {
		{"Singleplayer", .enter = MEN_SINGLEPLAYER},
		{"Multiplayer", .enter = MEN_MULTIPLAYER},
		{"Options", .enter = MEN_OPTIONS},
		{"Exit", .button = instaquit},
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
		{"Resolution: %dx%d", .disable_if = get_fullscreen, .format = fmt_resolution, .flip = toggle_resolution},
		{"Fullscreen: %s", .format = fmt_fullscreen, .flip = toggle_fullscreen},
		{"Vsync: %s", .format = fmt_vsync, .flip = toggle_vsync},
		{},
		{"Master Volume: %.0f%%", .format = fmt_volume, .flip = toggle_volume},
		{"Sound Volume: %.0f%%", .format = fmt_sound_volume, .flip = toggle_sound_volume},
		{"Music Volume: %.0f%%", .format = fmt_music_volume, .flip = toggle_music_volume},
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
		{"Host!", .button = do_host_fr},
	},
	[MEN_JOIN_LOBBY] = {
		// FIXME: Add explicit lobby joining to NutPunch.
		{"Lobby ID: %s", .format = fmt_lobby, EDIT(CLIENT.lobby.name)},
		{},
		{"Join!", .button = do_join_fr},
	},
	[MEN_FIND_LOBBY] = {
		{NULL, .disabled = true},
		// FIXME: Display lobbies with player counts
	},
	[MEN_JOINING_LOBBY] = {
		{"Joining lobby \"%s\"", .disabled = true, .format = fmt_lobby},
	},
	[MEN_LOBBY] = {
		{"%s", .disabled = true, .format = fmt_active_lobby},
		{},
		{"Players: %d", .format = fmt_players, .flip = set_players},
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

static void maybe_save_config(MenuType next) {
	if (next != MEN_OPTIONS && next != MEN_CONTROLS)
		save_config();
}

static void cleanup_lobby_list(MenuType next) {
	disconnect();
	for (int i = 0; i < MAX_OPTIONS; i++) {
		OPTIONS[MEN_FIND_LOBBY][i].name = i ? NULL : NO_LOBBIES_FOUND;
		OPTIONS[MEN_FIND_LOBBY][i].disabled = true;
		OPTIONS[MEN_FIND_LOBBY][i].button = NULL;
	}
}

static void maybe_play_title(MenuType next) {
	if (next == MEN_MAIN)
		play_generic_track("title", PLAY_LOOPING);
}

static void update_intro() {
	if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER))
		set_menu(MEN_MAIN);
}

static void update_find_lobbies() {
	for (int i = 0; i < MAX_OPTIONS; i++) {
		if (get_lobby_count() <= 0) {
			OPTIONS[MEN_FIND_LOBBY][i].name = i ? NULL : NO_LOBBIES_FOUND;
			OPTIONS[MEN_FIND_LOBBY][i].disabled = true;
			OPTIONS[MEN_FIND_LOBBY][i].button = NULL;
		} else if (i >= get_lobby_count()) {
			OPTIONS[MEN_FIND_LOBBY][i].name = NULL;
			OPTIONS[MEN_FIND_LOBBY][i].disabled = true;
			OPTIONS[MEN_FIND_LOBBY][i].button = NULL;
		} else {
			OPTIONS[MEN_FIND_LOBBY][i].name = get_lobby(i);
			OPTIONS[MEN_FIND_LOBBY][i].disabled = false;
			OPTIONS[MEN_FIND_LOBBY][i].button = join_found_lobby;
		}
	}
}

static void update_joining_lobby() {
	const int status = find_lobby();
	if (status < 0) {
		WTF("Lobby \"%s\" fail: %s", CLIENT.lobby.name, net_error());
		play_generic_sound("disconnect");
		prev_menu();
	} else if (status > 0) {
		play_generic_sound("connect");
		set_menu(MEN_LOBBY);
	}
}

static void update_inlobby() {
	if (is_connected())
		return;
	WTF("Lobby \"%s\" fail: %s", CLIENT.lobby.name, net_error());
	play_generic_sound("disconnect");
	prev_menu();
}

static void run_disableif() {
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[cur_menu][i];
		if (option->disable_if != NULL)
			option->disabled = option->disable_if();
	}
}

void update_menu() {
	for (new_frame(); got_ticks(); next_tick()) {
		int last_menu = cur_menu;
		if (MENUS[cur_menu].update != NULL)
			MENUS[cur_menu].update();
		if (last_menu != cur_menu)
			continue;
		run_disableif();

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
		if (kb_pressed(KB_UI_ENTER)) {
			const Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
			if (opt->disabled)
				goto NO_SELECT_CYKA;

			play_generic_sound("select");
			if (opt->button != NULL)
				opt->button();
			if (opt->edit != NULL)
				start_typing(opt->edit, opt->edit_size); // FIXME: Play "select" when pressing Enter or
				                                         //        backspace while typing
			if (opt->enter != MEN_NULL)
				set_menu(opt->enter);
		}

	NO_SELECT_CYKA:
		int flip = kb_repeated(KB_UI_RIGHT) - kb_repeated(KB_UI_LEFT);
		if (flip != 0) {
			const Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
			if (opt->flip != NULL && !opt->disabled) {
				play_generic_sound("select");
				opt->flip(flip);
			}
		}

		if (!kb_pressed(KB_PAUSE))
			continue; // de-nesting
		const char* sound = MENUS[cur_menu].back_sound;
		if (prev_menu())
			play_generic_sound(sound == NULL ? "select" : sound);
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

	if (cur_menu == MEN_FIND_LOBBY) {
		batch_cursor(SCREEN_WIDTH - 48, 24, 0);
		batch_color(ALPHA(128));
		batch_string("main", 24, ALIGN(FA_RIGHT, FA_TOP), fmt("Server: %s", get_hostname()));
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

	if (MENUS[cur_menu].from != next_menu) {
		if (MENUS[next_menu].noreturn)
			MENUS[next_menu].from = MEN_NULL;
		else if (MENUS[cur_menu].ghost)
			MENUS[next_menu].from = MENUS[cur_menu].from;
		else
			MENUS[next_menu].from = cur_menu;
	}

	if (MENUS[cur_menu].leave != NULL)
		MENUS[cur_menu].leave(next_menu);
	cur_menu = next_menu;
	if (MENUS[cur_menu].enter != NULL)
		MENUS[cur_menu].enter();

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

bool prev_menu() {
	return set_menu(MENUS[cur_menu].from);
}

static float volume_toggle_impl(float volume, int flip) {
	const float delta = (float)flip * 0.1f;

	if (delta > 0.f) {
		if (volume < 0.99f && (volume + delta) >= 1.f)
			volume = 1.f;
		else if (volume >= 0.99f && (volume + delta) >= 1.f)
			volume = 0.f;
		else
			volume += delta;
	} else if (volume > 0.01f && (volume + delta) < 0.f)
		volume = 0.f;
	else if (volume <= 0.01f && (volume + delta) <= 0.f)
		volume = 1.f;
	else
		volume += delta;

	return volume;
}
