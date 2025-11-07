#include <SDL3/SDL_misc.h>

#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_game.h"
#include "K_input.h"
#include "K_menu.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

static const char* credits[] = {
	"Credits",
	NULL,
	"[Mario Forever]",
	"Michal Gdaniec",
	"Maurycy Zarzycki",
	NULL,
	"[Programming]",
	"toggins",
	"nonk",
	NULL,
	"[Beta Testing]",
	"CST1229",
	"LooPeR231",
	"miyameon",
	"ReflexGURU",
	"Usered",
	NULL,
	"[Special Thanks]",
	"HeatXD",
	"Xykijun",
};
static float credits_y = SCREEN_HEIGHT;

static MenuType cur_menu = MEN_NULL;
static int kevin_state = 0, kevin_time = 0;
static float kevin_lerp = 0.f;

static GameWinner winners[MAX_PLAYERS] = {0};

static float volume_toggle_impl(float, int);
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
#define BIND_OPTION(fname, kb)                                                                                         \
	static void rebind_##fname() {                                                                                 \
		start_scanning(kb);                                                                                    \
	}

// Main Menu
extern bool permadeath;
static void instaquit() {
	permadeath = true;
}

// Singleplayer
FMT_OPTION(level, CLIENT.game.level);

static void play_singleplayer() {
	play_generic_sound("enter");

	GameContext ctx;
	setup_game_context(&ctx, CLIENT.game.level, GF_SINGLE | GF_TRY_KEVIN);
	start_game(&ctx);
}

// Multiplayer Note
static void open_troubleshooting_url() {
	SDL_OpenURL("https://github.com/Schwungus/nutpunch?tab=readme-ov-file#troubleshooting");
}

static void read_multiplayer_note() {
	CLIENT.user.aware = true;
	set_menu(MEN_MULTIPLAYER);
}

static void screw_multiplayer_note() {
	read_multiplayer_note();
	save_config();
}

// Multiplayer

// Host Lobby
FMT_OPTION(lobby, CLIENT.lobby.name);
FMT_OPTION(lobby_public, CLIENT.lobby.public ? "Public" : "Private");
FMT_OPTION(players, CLIENT.game.players);

static void toggle_lobby_public(int flip) {
	CLIENT.lobby.public = !CLIENT.lobby.public;
}

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
FMT_OPTION(active_lobby, get_lobby_id(), in_public_lobby() ? "" : " (Private)");
FMT_OPTION(lobby_start, is_host() ? "Start!" : "Waiting for host");

static void set_players(int flip) {
	if (flip >= 0)
		CLIENT.game.players = (int8_t)(CLIENT.game.players >= MAX_PLAYERS ? 2 : (CLIENT.game.players + 1));
	else
		CLIENT.game.players = (int8_t)(CLIENT.game.players <= 2 ? MAX_PLAYERS : (CLIENT.game.players - 1));
}

FMT_OPTION(waiting, (get_peer_count() >= CLIENT.game.players) ? "Starting!" : "Waiting for players");

// Options
FMT_OPTION(name, CLIENT.user.name);
FMT_OPTION(skin, CLIENT.user.skin);

FMT_OPTION(delay, CLIENT.input.delay);
static void set_delay(int flip) {
	if (flip >= 0)
		CLIENT.input.delay = (uint8_t)(CLIENT.input.delay >= MAX_INPUT_DELAY ? 0 : (CLIENT.input.delay + 1));
	else
		CLIENT.input.delay = (uint8_t)(CLIENT.input.delay <= 0 ? MAX_INPUT_DELAY : (CLIENT.input.delay - 1));
}

static const char* fmt_resolution(const char* base) {
	int width = 0, height = 0;
	get_resolution(&width, &height);
	return fmt(base, width, height);
}

static void toggle_resolution(int flip) {
	int width = 0, height = 0;
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

FMT_OPTION(filter, CLIENT.video.filter ? "On" : "Off");
static void toggle_filter(int flip) {
	CLIENT.video.filter = !CLIENT.video.filter;
}

FMT_OPTION(volume, get_volume() * 100);
VOLUME_OPTION(volume);

FMT_OPTION(sound_volume, get_sound_volume() * 100);
VOLUME_OPTION(sound_volume);

FMT_OPTION(music_volume, get_music_volume() * 100);
VOLUME_OPTION(music_volume);

FMT_OPTION(background, CLIENT.audio.background ? "Yes" : "No");
static void toggle_background(int flip) {
	CLIENT.audio.background = !CLIENT.audio.background;
}

// Controls
FMT_OPTION(device, input_device());

#define FMT_KEYBIND(fname, kb) FMT_OPTION(fname, (scanning_what() == kb) ? "(Press any key)" : kb_label(kb))
FMT_KEYBIND(up, KB_UP);
BIND_OPTION(up, KB_UP);
FMT_KEYBIND(left, KB_LEFT);
BIND_OPTION(left, KB_LEFT);
FMT_KEYBIND(down, KB_DOWN);
BIND_OPTION(down, KB_DOWN);
FMT_KEYBIND(right, KB_RIGHT);
BIND_OPTION(right, KB_RIGHT);
FMT_KEYBIND(jump, KB_JUMP);
BIND_OPTION(jump, KB_JUMP);
FMT_KEYBIND(fire, KB_FIRE);
BIND_OPTION(fire, KB_FIRE);
FMT_KEYBIND(run, KB_RUN);
BIND_OPTION(run, KB_RUN);
FMT_KEYBIND(chat, KB_CHAT);
BIND_OPTION(chat, KB_CHAT);
#undef FMT_KEYBIND

#undef FMT_OPTION
#undef BOOL_OPTION
#undef VOLUME_OPTION
#undef BIND_OPTION

static void update_intro(), reset_credits(), enter_multiplayer_note(), update_find_lobbies(), update_joining_lobby(),
	update_inlobby();
static void maybe_save_config(MenuType), cleanup_lobby_list(MenuType), play_title_fr(MenuType),
	maybe_play_title(MenuType), maybe_disconnect(MenuType);

#define GHOST .ghost = true
#define NORETURN .noreturn = true
static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {NORETURN, .leave = maybe_play_title},
	[MEN_ERROR] = {"Error", GHOST},
	[MEN_RESULTS] = {"", GHOST, .leave = play_title_fr},
	[MEN_INTRO] = {NORETURN, .update = update_intro, .leave = maybe_play_title},
	[MEN_MAIN] = {"Mario Together", NORETURN, .enter = reset_credits},
	[MEN_SINGLEPLAYER] = {"Singleplayer"},
	[MEN_MULTIPLAYER_NOTE] = {"Read This First!", GHOST, .enter = enter_multiplayer_note},
	[MEN_MULTIPLAYER] = {"Multiplayer"},
	[MEN_HOST_LOBBY] = {"Host Lobby"},
	[MEN_JOIN_LOBBY] = {"Join a Lobby"},
	[MEN_FIND_LOBBY]
	= {"Find Lobbies", .update = update_find_lobbies, .enter = list_lobbies, .leave = cleanup_lobby_list},
	[MEN_JOINING_LOBBY] = {"Please wait...", .update = update_joining_lobby, .back_sound = "disconnect",
		      .leave = maybe_disconnect, GHOST},
	[MEN_LOBBY] = {"Lobby", .back_sound = "disconnect", .update = update_inlobby, .leave = disconnect, GHOST},
	[MEN_OPTIONS] = {"Options", .leave = maybe_save_config},
	[MEN_CONTROLS] = {"Controls", .leave = maybe_save_config},
};

// Main Menu
static void reset_credits() {
	credits_y = SCREEN_HEIGHT;
}

// Find lobbies
static void join_found_lobby() {
	const char* lober = get_lobby((int)MENUS[MEN_FIND_LOBBY].option);
	if (lober != NULL) {
		join_lobby(lober);
		set_menu(MEN_JOINING_LOBBY);
	}
}

#define EDIT(var) .edit = (var), .edit_size = sizeof(var)
#define TOGGLE(fname) .flip = toggle_##fname
#define FORMAT(fname) .format = fmt_##fname
#define REBIND(fname) .button = rebind_##fname
#define DISABLE .disabled = true

static const char* NO_LOBBIES_FOUND = "No lobbies found";

static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_ERROR] = {},
	[MEN_RESULTS] = {},
	[MEN_MAIN] = {
		{"Singleplayer", .enter = MEN_SINGLEPLAYER},
		{"Multiplayer", .enter = MEN_MULTIPLAYER_NOTE},
		{"Options", .enter = MEN_OPTIONS},
		{"Exit", .button = instaquit},
	},
	[MEN_SINGLEPLAYER] = {
		{"Level: %s", FORMAT(level), EDIT(CLIENT.game.level)},
		{},
		{"Start!", .button = play_singleplayer},
	},
	[MEN_MULTIPLAYER_NOTE] = {
		{"Mario Together uses NutPunch to make peer-to-peer", DISABLE},
		{"connections through UDP hole-punching.", DISABLE},
		{"For a smooth experience, make sure that:", DISABLE},
		{"  1. You and your friends' game is up to date.", DISABLE},
		{"  2. Your VPN client is off.", DISABLE},
		{"  3. Your firewall isn't blocking the game.", DISABLE},
		{},
		{"If you are able to join lobbies AND see other players in", DISABLE},
		{"them, then it should be working.", DISABLE},
		{},
		{"OK", .button = read_multiplayer_note},
		{"Don't Show Again", .button = screw_multiplayer_note},
		{"NutPunch Troubleshooting (opens browser)", .button = open_troubleshooting_url},
	},
	[MEN_MULTIPLAYER] = {
		{"Host Lobby", .enter = MEN_HOST_LOBBY},
		{"Join a Lobby", .enter = MEN_JOIN_LOBBY},
		{"Find Lobbies", .enter = MEN_FIND_LOBBY},
	},
	[MEN_OPTIONS] = {
		{"Name: %s", FORMAT(name), EDIT(CLIENT.user.name)},
		{"Skin: %s", FORMAT(skin), EDIT(CLIENT.user.skin)},
		{},
		{"Controls", .enter = MEN_CONTROLS},
		{"Input Delay: %i", FORMAT(delay), .flip = set_delay},
		{},
		{"Resolution: %dx%d", .disable_if = get_fullscreen, FORMAT(resolution), TOGGLE(resolution)},
		{"Fullscreen: %s", FORMAT(fullscreen), TOGGLE(fullscreen)},
		{"Vsync: %s", FORMAT(vsync), TOGGLE(vsync)},
		{"Texture Filter: %s", FORMAT(filter), TOGGLE(filter)},
		{},
		{"Master Volume: %.0f%%", FORMAT(volume), TOGGLE(volume)},
		{"Sound Volume: %.0f%%", FORMAT(sound_volume), TOGGLE(sound_volume)},
		{"Music Volume: %.0f%%", FORMAT(music_volume), TOGGLE(music_volume)},
		{"Play in Background: %s", FORMAT(background), TOGGLE(background)},
	},
	[MEN_CONTROLS] = {
		{"Device: %s", DISABLE, FORMAT(device)},
		{},
		{"Up: %s", FORMAT(up), REBIND(up)},
		{"Left: %s", FORMAT(left), REBIND(left)},
		{"Down: %s", FORMAT(down), REBIND(down)},
		{"Right: %s", FORMAT(right), REBIND(right)},
		{},
		{"Jump: %s", FORMAT(jump), REBIND(jump)},
		{"Run: %s", FORMAT(run), REBIND(run)},
		{"Fire: %s", FORMAT(fire), REBIND(fire)},
		{},
		{"Chat: %s", FORMAT(chat), REBIND(chat)},
	},
	[MEN_HOST_LOBBY] = {
		{"Lobby ID: %s", FORMAT(lobby), EDIT(CLIENT.lobby.name)},
		{"Visibility: %s", FORMAT(lobby_public), TOGGLE(lobby_public)},
		{},
		{"Players: %d", FORMAT(players), .flip = set_players},
		{"Level: %s", FORMAT(level), EDIT(CLIENT.game.level)},
		{},
		{"Host!", .button = do_host_fr},
	},
	[MEN_JOIN_LOBBY] = {
		{"Lobby ID: %s", FORMAT(lobby), EDIT(CLIENT.lobby.name)},
		{},
		{"Join!", .button = do_join_fr},
	},
	[MEN_FIND_LOBBY] = {
		{NULL, DISABLE},
		// FIXME: Display lobbies with player counts!
		//        Lobby lists return names, but no way to get their count.
	},
	[MEN_JOINING_LOBBY] = {
		{"Joining lobby \"%s\"", DISABLE, FORMAT(active_lobby)},
	},
	[MEN_LOBBY] = {
		{"%s%s", DISABLE, FORMAT(active_lobby)},
		{},
		{"Players: %d", DISABLE, FORMAT(players)},
		{"Level: %s", DISABLE, FORMAT(level)},
		{},
		{"%s", DISABLE, FORMAT(waiting)},
	},
};

void start_menu(bool skip_intro) {
	from_scratch();

	load_texture("ui/disclaimer");
	load_texture("ui/background");
	load_texture("ui/shortcut");

	load_font("main");
	load_font("hud");

	load_sound("switch");
	load_sound("select");
	load_sound("toggle");
	load_sound("enter");
	load_sound("on");
	load_sound("off");
	load_sound("kevin_type");
	load_sound("kevin_on");
	load_sound("thwomp");
	load_sound("connect");
	load_sound("disconnect");

	load_track("title");
	load_track("human_like_predator");
	load_track("yi_score");

	set_menu(skip_intro ? MEN_MAIN : MEN_INTRO);
}

static void enter_multiplayer_note() {
	if (CLIENT.user.aware)
		set_menu(MEN_MULTIPLAYER);
}

static void maybe_save_config(MenuType next) {
	if (next != MEN_OPTIONS && next != MEN_CONTROLS)
		save_config();
}

static void cleanup_lobby_list(MenuType next) {
	maybe_disconnect(next);
	for (int i = 0; i < MAX_OPTIONS; i++) {
		OPTIONS[MEN_FIND_LOBBY][i].name = i ? NULL : NO_LOBBIES_FOUND;
		OPTIONS[MEN_FIND_LOBBY][i].disabled = true;
		OPTIONS[MEN_FIND_LOBBY][i].button = NULL;
	}
}

static void play_title_fr(MenuType next) {
	play_generic_track(CLIENT.game.kevin ? "human_like_predator" : "title", PLAY_LOOPING);
}

static void maybe_play_title(MenuType next) {
	if (next == MEN_MAIN && !CLIENT.game.kevin)
		play_title_fr(next);
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
		show_error("Failed to join lobby\n%s", net_error());
		play_generic_sound("disconnect");
	} else if (status > 0) {
		set_menu(MEN_LOBBY);
		play_generic_sound("connect");
	}
}

static void update_inlobby() {
	if (!is_connected()) {
		const char* error = net_error();
		if (error == NULL)
			prev_menu();
		else
			show_error("Lost connection\n%s", net_error());
		play_generic_sound("disconnect");
		return;
	}

	int num_peers = 0;
	for (int i = 0; i < MAX_PEERS; i++)
		num_peers += get_peer_name(i) != NULL;
	if (num_peers < CLIENT.game.players)
		return;
	play_generic_sound("enter");
	make_lobby_active();

	GameContext ctx;
	setup_game_context(&ctx, CLIENT.game.level, GF_TRY_KEVIN);
	ctx.num_players = CLIENT.game.players;
	start_game(&ctx);
}

static void maybe_disconnect(MenuType next) {
	if (next != MEN_LOBBY && next != MEN_JOINING_LOBBY)
		disconnect();
}

static void run_disableif() {
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[cur_menu][i];
		if (option->disable_if != NULL)
			option->disabled = option->disable_if();
	}
}

static const char* kevinstr = "";
static void keve() {
	switch (kevin_state) {
	default:
		kevinstr = "";
		break;
	case 1:
		kevinstr = "K";
		break;
	case 2:
		kevinstr = "KE";
		break;
	case 3:
		kevinstr = "KEV";
		break;
	case 4:
		kevinstr = "KEVI";
		break;
	case 5:
		kevinstr = "KEVIN";
		break;
	}
}

static void type_kevin(Keybind kb) {
	if (!kb_pressed(kb))
		return;

	if (kevin_state >= 4)
		CLIENT.game.kevin = true;
	else {
		play_generic_sound("kevin_type");
		++kevin_state;
		kevin_time = TICKRATE;
	}
	keve();
}

void update_menu() {
	for (new_frame(0); got_ticks(); next_tick()) {
		int last_menu = cur_menu, flip = 0;
		if (MENUS[cur_menu].update != NULL)
			MENUS[cur_menu].update();
		if (last_menu != cur_menu)
			continue;
		run_disableif();

		if (kevin_state < 5 && kevin_time > 0) {
			--kevin_time;
			if (kevin_time <= 0) {
				kevin_state = 0;
				keve();
			}
		}

		if (CLIENT.game.kevin && kevin_state < 5) {
			play_generic_sound("kevin_on");
			if (cur_menu != MEN_RESULTS)
				play_generic_track("human_like_predator", PLAY_LOOPING);
			kevin_state = 5;
			kevin_time = 0;
			keve();
		} else if (!CLIENT.game.kevin && kevin_state >= 5) {
			play_generic_sound("thwomp");
			if (cur_menu != MEN_RESULTS)
				play_generic_track("title", PLAY_LOOPING);
			kevin_state = 0;
			kevin_time = 0;
			keve();
		}

		if (!is_connected() && (cur_menu != MEN_JOINING_LOBBY && cur_menu != MEN_LOBBY))
			switch (kevin_state) {
			default:
				type_kevin(KB_KEVIN_K);
				break;
			case 1:
				type_kevin(KB_KEVIN_E);
				break;
			case 2:
				type_kevin(KB_KEVIN_V);
				break;
			case 3:
				type_kevin(KB_KEVIN_I);
				break;
			case 4:
				type_kevin(KB_KEVIN_N);
				break;
			case 5:
				if (kb_pressed(KB_KEVIN_BAIL))
					CLIENT.game.kevin = false;
				break;
			}

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
			Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
			if (opt->disabled)
				goto NO_SELECT_CYKA;

			play_generic_sound("select");
			if (opt->button != NULL)
				opt->button();
			else if (opt->flip != NULL)
				opt->flip(1);
			else if (opt->edit != NULL)
				start_typing(opt->edit, opt->edit_size); // FIXME: Play "select" when pressing Enter or
				                                         //        backspace while typing
			else if (opt->enter != MEN_NULL)
				set_menu(opt->enter);
		}

	NO_SELECT_CYKA:
		flip = kb_repeated(KB_UI_RIGHT) - kb_repeated(KB_UI_LEFT);
		if (flip != 0) {
			const Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
			if (opt->flip != NULL && !opt->disabled) {
				play_generic_sound("toggle");
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

	batch_start(ORIGO, 0.f, WHITE);
	batch_sprite("ui/background", NO_FLIP);

	const float lk = 0.125f * dt();
	kevin_lerp = glm_lerp(kevin_lerp, CLIENT.game.kevin, SDL_min(lk, 1.f));
	const GLubyte fade_alpha = (GLubyte)(kevin_lerp * 255.f);
	batch_cursor(XY(-SCREEN_WIDTH, 0));
	batch_colors((GLubyte[4][4]){
		{32, 0, 0, 0         },
                {32, 0, 0, 0         },
                {0,  0, 0, fade_alpha},
                {0,  0, 0, fade_alpha}
        });
	batch_rectangle(NULL, XY(SCREEN_WIDTH * 3, SCREEN_HEIGHT));
	batch_color(WHITE);

	Menu* menu = &MENUS[cur_menu];
	const float menu_y = 60.f;

	const float lx = 0.6f * dt();
	menu->cursor = glm_lerp(menu->cursor, (float)menu->option, SDL_min(lx, 1.f));
	if (!OPTIONS[cur_menu][menu->option].disabled) {
		batch_cursor(XY(44.f + (SDL_sinf(totalticks() / 5.f) * 4.f), menu_y + (menu->cursor * 24.f)));
		batch_align(FA_RIGHT, FA_TOP);
		batch_string("main", 24.f, ">");
	}

	batch_cursor(XY(48.f, 24.f));
	batch_color(RGBA(255, 144, 144, 192));
	batch_align(FA_LEFT, FA_TOP);
	batch_string("main", 24.f, menu->name);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[cur_menu][i];
		if (opt->name == NULL)
			continue;

		const float lx = 0.5f * dt();
		opt->hover = glm_lerp(opt->hover, (float)(menu->option == i && !opt->disabled), SDL_min(lx, 1.f));

		const char *suffix = "", *format = opt->format == NULL ? opt->name : opt->format(opt->name);
		if (opt->edit != NULL && typing_what() == opt->edit && SDL_fmodf(totalticks(), 30.f) < 16.f)
			suffix = "|";

		static char name[512] = {0}; // separate fmt buffer to prevent memory-related bugs
		SDL_snprintf(name, sizeof(name), "%s%s", format, suffix);

		const GLfloat x = 48.f + (opt->hover * 8.f);
		const GLfloat y = menu_y + ((GLfloat)i * 24.f);

		batch_cursor(XY(x, y));
		batch_color(ALPHA(opt->disabled ? 128 : 255));
		batch_align(FA_LEFT, FA_TOP);
		batch_string("main", 24.f, name);

		if (opt->enter != MEN_NULL) {
			batch_cursor(XY(x + string_width("main", 24.f, name) + 4.f, y + 8.f));
			batch_sprite("ui/shortcut", NO_FLIP);
		}
	}

	if (menu->from != MEN_NULL) {
		const char *btn = (cur_menu == MEN_JOINING_LOBBY || cur_menu == MEN_LOBBY) ? "Disconnect" : "Back",
			   *indicator = fmt("[%s] %s", kb_label(KB_PAUSE), btn);
		batch_cursor(XY(48.f, SCREEN_HEIGHT - 24.f));
		batch_color(ALPHA(128));
		batch_align(FA_LEFT, FA_BOTTOM);
		batch_string("main", 24.f, indicator);
	}

	switch (cur_menu) {
	default:
		break;

	case MEN_MAIN: {
		const int num_lines = sizeof(credits) / sizeof(*credits);

		credits_y -= dt() * 0.5f;
		if (credits_y <= ((float)num_lines * -18.f))
			credits_y = SCREEN_HEIGHT;

		for (int i = 0; i < num_lines; i++) {
			const float y = credits_y + ((float)i * 18.f);
			if (y >= (SCREEN_HEIGHT - 18.f) || y <= 0.f)
				continue;

			const float top = 200.f, bottom = SCREEN_HEIGHT - 200.f;
			if ((y + 18.f) > bottom)
				batch_color(ALPHA(128 - (GLubyte)(((y + 18.f - bottom) / 200.f) * 128.f)));
			else if (y < top)
				batch_color(ALPHA(128 - (GLubyte)(((top - y) / 200.f) * 128.f)));
			else
				batch_color(ALPHA(128));

			batch_cursor(XY(SCREEN_WIDTH - 48.f, y));
			batch_align(FA_RIGHT, FA_TOP);
			batch_string("main", 18.f, credits[i]);
		}
		break;
	}

	case MEN_FIND_LOBBY: {
		batch_cursor(XY(SCREEN_WIDTH - 48.f, 24.f));
		batch_color(ALPHA(128));
		batch_align(FA_RIGHT, FA_TOP);
		batch_string("main", 24.f, fmt("Server: %s", get_hostname()));
		break;
	}

	case MEN_LOBBY: {
		batch_color(RGBA(255, 144, 144, 192));
		batch_cursor(XY(SCREEN_WIDTH - 48.f, 24.f));
		batch_align(FA_RIGHT, FA_TOP);
		int num_peers = get_peer_count();
		batch_string("main", 24.f, fmt("Players (%i / %i)", num_peers, CLIENT.game.players));

		batch_color(ALPHA(128));
		GLfloat y = 60.f;
		int idx = 1;
		for (int i = 0; i < MAX_PEERS; i++) {
			if (!peer_exists(i))
				continue;
			batch_cursor(XY(SCREEN_WIDTH - 48.f, y));
			batch_align(FA_RIGHT, FA_TOP);
			batch_string("main", 24.f, fmt("%i. %s", idx, get_peer_name(i)));
			++idx;
			y += 24.f;
		}

		break;
	}

	case MEN_RESULTS: {
		batch_cursor(XY(HALF_SCREEN_WIDTH, 174.f));
		batch_color(RGBA(255, 144, 144, 192));
		batch_align(FA_CENTER, FA_TOP);
		batch_string("main", 24.f, "Scoreboard");

		for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
			if (winners[i].name[0] == '\0')
				continue;
			batch_cursor(XY(HALF_SCREEN_WIDTH, 210.f + (i * 24.f)));
			batch_color((i == 0) ? RGBA(255, 255, 0, 192) : ((i >= 3) ? ALPHA(128) : ALPHA(192)));
			batch_string("main", 24.f,
				fmt("%i. %s x%i, %i pts", i + 1, winners[i].name, winners[i].lives, winners[i].score));
		}

		break;
	}
	}

	batch_cursor(XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 48.f));
	batch_color(RGB(255, 96, 96));
	batch_align(FA_CENTER, FA_BOTTOM);
	batch_string("main", 24.f + ((16.f + SDL_sinf(totalticks() / 32.f) * 4.f) * kevin_lerp), kevinstr);
	if (cur_menu != MEN_JOINING_LOBBY && cur_menu != MEN_LOBBY) {
		batch_color(ALPHA(128 * kevin_lerp));
		batch_align(FA_CENTER, FA_TOP);
		batch_string("main", 24, fmt("[%s] Deactivate", kb_label(KB_KEVIN_BAIL)));
	}

	goto jobwelldone;

draw_intro:
	clear_color(0, 0, 0, 1);

	float a = 1.f, ticks = totalticks();
	if (ticks < 25.f)
		a = ticks / 25.f;
	if (ticks > 125.f)
		a = 1.f - ((ticks - 100.f) / 25.f);

	batch_alpha_test(0);
	batch_cursor(XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT));
	batch_color(ALPHA(a * 255));
	batch_sprite("ui/disclaimer", NO_FLIP);
	batch_alpha_test(0.5f);

jobwelldone:
	stop_drawing();
}

void show_error(const char* fmt, ...) {
	static char buf[128] = {0};
	va_list args = {0};

	va_start(args, fmt);
	SDL_strlcpy(buf, vfmt(fmt, args), sizeof(buf));
	va_end(args);

	char linum = 0, *sep = "\n", *state = buf, *line = SDL_strtok_r(buf, sep, &state);
	for (int k = 0; k < MAX_OPTIONS; k++) {
		OPTIONS[MEN_ERROR][k].name = "";
		OPTIONS[MEN_ERROR][k].disabled = true;
	}
	while (line) {
		OPTIONS[MEN_ERROR][linum++].name = line;
		line = SDL_strtok_r(NULL, sep, &state);
	}

	set_menu(MEN_ERROR);
}

void populate_results() {
	SDL_memset(&winners, 0, sizeof(winners));
	for (PlayerID i = 0; i < numplayers(); i++) {
		const char* name = get_peer_name(player_to_peer(i));
		SDL_strlcpy(winners[i].name, (name == NULL) ? fmt("Player %i", i + 1) : name, sizeof(winners[i].name));
	}
}

void show_results() {
	for (int i = 0; i < MAX_OPTIONS; i++) {
		OPTIONS[MEN_RESULTS][i].name = "";
		OPTIONS[MEN_RESULTS][i].disabled = true;
	}

	char idx = 0;

	// Endgame reason
	bool all_dead = true;
	for (PlayerID i = 0; i < numplayers(); i++) {
		const GamePlayer* player = get_player(i);
		if (player->lives >= 0L) {
			all_dead = false;
			break;
		}
	}

	MENUS[MEN_RESULTS].name = all_dead ? "Game Over" : "World Completed";
	OPTIONS[MEN_RESULTS][idx++].name
		= all_dead ? ((game_state.flags & GF_KEVIN) ? "Nobody escaped Kevin..." : "No players remaining")
	                   : ((game_state.flags & GF_KEVIN) ? "Kevin will be back..." : "All levels cleared");

	idx++;

	// Scoreboard
	for (PlayerID i = 0; i < numplayers(); i++) {
		const GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;
		winners[i].lives = player->lives;
		winners[i].score = player->score;
	}

	GameWinner cmp[2] = {0};
	for (PlayerID i = 0; i < numplayers(); i++)
		for (PlayerID j = 0; j < (numplayers() - 1); j++) {
			if (winners[j].score >= winners[j + 1].score)
				continue;

			SDL_memcpy(&cmp[0], &winners[j], sizeof(cmp[0]));
			SDL_memcpy(&cmp[1], &winners[j + 1], sizeof(cmp[1]));

			SDL_memcpy(&winners[j], &cmp[1], sizeof(cmp[1]));
			SDL_memcpy(&winners[j + 1], &cmp[0], sizeof(cmp[0]));
		}

	if (all_dead)
		stop_generic_track();
	else
		play_generic_track("yi_score", PLAY_LOOPING);
	set_menu(MEN_RESULTS);
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
