#include <SDL3/SDL_misc.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_discord.h"
#include "K_file.h"
#include "K_menu.h"
#include "K_misc.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "SDL3/SDL_filesystem.h"

extern Bool permadeath, quickstart;

// =======
// CREDITS
// =======

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
	"Collin",
	"CST1229",
	"Jaydex",
	"LooPeR231",
	"Mightcer",
	"miyameon",
	"ReflexGURU",
	"Usered",
	NULL,
	"[Special Thanks]",
	"HeatXD",
	"Xykijun",
};
static float credits_y = SCREEN_HEIGHT;

// =======
// SECRETS
// =======

static Secret SECRETS[SECR_SIZE] = {
	[SECR_KEVIN] = {
		.name = "KEVIN",
		.inputs = {KB_SECRET_K, KB_SECRET_E, KB_SECRET_V, KB_SECRET_I, KB_SECRET_N, NULLBIND},
		.cmd = &CLIENT.game.kevin,
		.sound = "kevin_on", .track = "human_like_predator",
	},
	[SECR_FRED] = {
		.name = "FRED",
		.inputs = {KB_SECRET_F, KB_SECRET_R, KB_SECRET_E, KB_SECRET_D, NULLBIND},
		.cmd = &CLIENT.game.fred,
		.sound = "clone_dead2", .track = "bassbin_bangin",
	},
};

static int secret_state = 0;
static float secret_lerp = 0.f;
static int last_secret = -1;

static void load_secrets() {
	for (SecretType i = 0; i < SECR_SIZE; i++) {
		load_sound(SECRETS[i].sound, FALSE);
		load_track(SECRETS[i].track, TRUE);
	}

	load_sound("type", FALSE);
	load_sound("thwomp", FALSE);
	load_track("it_makes_me_burn", TRUE);
}

static void update_menu_track();
static void activate_secret(SecretType idx) {
	SECRETS[idx].active = TRUE;
	SECRETS[idx].state = SECRETS[idx].type_time = 0;
	*(SECRETS[idx].cmd) = TRUE;

	if (last_secret == idx)
		last_secret = -1;
	play_generic_sound(SECRETS[idx].sound);
	update_menu_track();
	push_lobby_data();
}

static void deactivate_secret(SecretType idx) {
	SECRETS[idx].active = FALSE;
	SECRETS[idx].state = SECRETS[idx].type_time = 0;
	*(SECRETS[idx].cmd) = FALSE;

	if (last_secret == idx)
		last_secret = -1;
	update_menu_track();
	push_lobby_data();
}

static MenuType cur_menu;
static void update_secrets() {
	const Bool handicapped = (NutPunch_IsReady() && !NutPunch_IsMaster()) || cur_menu == MEN_INTRO
	                         || cur_menu == MEN_JOINING_LOBBY;
	for (SecretType i = 0; i < SECR_SIZE; i++) {
		Secret* secret = &SECRETS[i];

		if (secret->active) {
			if (!*secret->cmd)
				deactivate_secret(i);
			continue;
		} else if (*secret->cmd) {
			activate_secret(i);
			continue;
		}

		if (secret->type_time > 0)
			if (--secret->type_time <= 0) {
				secret->state = 0;
				if (last_secret == i)
					last_secret = -1;
				continue;
			}

		if (handicapped || (last_secret >= 0 && last_secret != i) || !kb_pressed(secret->inputs[secret->state]))
			continue;
		if (secret->inputs[++secret->state] != NULLBIND) {
			last_secret = i;
			secret->type_time = TICKRATE;
			play_generic_sound("type");
			continue;
		}

		activate_secret(i);
	}

	if (handicapped || !kb_pressed(KB_SECRET_BAIL))
		return;

	int cancelled = 0;
	for (SecretType i = 0; i < SECR_SIZE; i++) {
		if (!SECRETS[i].active)
			continue;
		deactivate_secret(i);
		++cancelled;
	}
	if (cancelled > 0)
		play_generic_sound("thwomp");
}

// ===========================================================
// THE ACTUAL THING WE'RE SUPPOSED TO IMPLEMENT IN THIS SOURCE
// ===========================================================

static MenuType cur_menu = MEN_NULL;

static GameWinner winners[MAX_PLAYERS] = {0};
static Bool losers = FALSE;

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
static void instaquit() {
	permadeath = TRUE;
}

// Singleplayer
FMT_OPTION(level, CLIENT.game.level);

static void play_singleplayer() {
	play_generic_sound("enter");

	GameContext* ctx = init_game_context();
	SDL_strlcpy(ctx->level, CLIENT.game.level, sizeof(ctx->level));
	ctx->flags |= GF_TRY_HELL;
	start_game();
}

// Multiplayer Note
static void open_troubleshooting_url() {
	SDL_OpenURL("https://github.com/Schwungus/nutpunch?tab=readme-ov-file#troubleshooting");
}

static void read_multiplayer_note() {
	CLIENT.user.aware = TRUE;
	set_menu(MEN_MULTIPLAYER);
}

static void screw_multiplayer_note() {
	read_multiplayer_note();
	save_config();
}

// Multiplayer

// Host Lobby
FMT_OPTION(lobby, CLIENT.lobby.name);
FMT_OPTION(lobby_public, CLIENT.lobby.public ? "Public" : "Unlisted");
FMT_OPTION(players, CLIENT.game.players);

static void toggle_lobby_public(int flip) {
	CLIENT.lobby.public = !CLIENT.lobby.public;
}

static void do_host_fr() {
	host_lobby(CLIENT.lobby.name);
	set_menu(MEN_JOINING_LOBBY);
}

static void maybe_reset_lobby_name() {
	if (!SDL_strlen(CLIENT.lobby.name))
		SDL_snprintf(CLIENT.lobby.name, sizeof(CLIENT.lobby.name), "%s's Lobby", CLIENT.user.name);
}

// Join a Lobby
static void do_join_fr() {
	if (CLIENT.lobby.name[0] == '\0') {
		play_generic_sound("bump");
		return;
	}
	join_lobby(CLIENT.lobby.name);
	set_menu(MEN_JOINING_LOBBY);
}

// Lobby
FMT_OPTION(active_lobby, get_lobby_id(), in_public_lobby() ? "" : " (Unlisted)");
FMT_OPTION(lobby_limit, NutPunch_GetMaxPlayers());
FMT_OPTION(spectate, i_am_spectating() ? "Spectator" : "Player");

// Ingame
FMT_OPTION(restart, NutPunch_IsReady() ? "(Can't restart in multiplayer!)" : "Restart Level (-1 Life)")
FMT_OPTION(exit, NutPunch_IsReady() ? "Return to Lobby" : "Return to Title")

static void set_players(int flip) {
	CLIENT.game.players
		= (PlayerID)((flip >= 0) ? ((CLIENT.game.players >= MAX_PLAYERS) ? 2 : (CLIENT.game.players + 1))
					 : ((CLIENT.game.players <= 2) ? MAX_PLAYERS : (CLIENT.game.players - 1)));
}

static void set_spectate(int flip) {
	const Bool toggle = !i_am_spectating();
	NutPunch_PeerSet("SPEC", sizeof(Bool), &toggle);
	push_user_data();
}

static void start_multiplayer() {
	if (!NutPunch_IsMaster())
		return;

	char* data = net_buffer();
	*(PacketType*)data = PT_START;
	for (int i = 0; i < MAX_PEERS; i++)
		if (NutPunch_PeerAlive(i))
			NutPunch_SendReliably(PCH_LOBBY, i, data, sizeof(PacketType));

	// FIXME: Starting a session right after sending a start packet will
	//        delay the packet until the host has loaded in. This sucks.
	start_online_game(TRUE);
}

static Bool is_client() {
	return !NutPunch_IsMaster();
}

static Bool disable_start() {
	if (NutPunch_IsMaster())
		for (int i = 0; i < MAX_PEERS; i++)
			if (NutPunch_PeerAlive(i) && !peer_is_spectating(i))
				return FALSE;
	return TRUE;
}

FMT_OPTION(start, NutPunch_IsMaster() ? "Start!" : "Waiting for host");

// Options
FMT_OPTION(name, CLIENT.user.name);

FMT_OPTION(delay, CLIENT.input.delay);
static void set_input_delay(int flip) {
	CLIENT.input.delay = (flip >= 0)
	                             ? (Uint8)((CLIENT.input.delay >= MAX_INPUT_DELAY) ? 0 : (CLIENT.input.delay + 1))
	                             : (Uint8)((CLIENT.input.delay <= 0) ? MAX_INPUT_DELAY : (CLIENT.input.delay - 1));
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
			set_resolution(SCREEN_WIDTH * 1.5, SCREEN_HEIGHT * 1.5, TRUE);
		else if (scale >= 1.25 && scale < 1.75)
			set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, TRUE);
		else if (scale >= 1.75)
			set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
	} else if (scale < 1.25)
		set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, TRUE);
	else if (scale >= 1.25 && scale < 1.75)
		set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
	else if (scale >= 1.75)
		set_resolution(SCREEN_WIDTH * 1.5, SCREEN_HEIGHT * 1.5, TRUE);
}

FMT_OPTION(fullscreen, get_fullscreen() ? "On" : "Off");
BOOL_OPTION(fullscreen, get_fullscreen, set_fullscreen);

FMT_OPTION(framerate, get_framerate());
static void flip_framerate(int flip) {
	static const int ranges[] = {0, 30, 50, 60, 75, 120, 144, 165, 180, 240, 360};
	const int fps = get_framerate(), len = entries(ranges);
	if (flip >= 0) {
		for (int i = 0; i < len; i++)
			if (fps < ranges[i]) {
				set_framerate(ranges[i]);
				return;
			}
		set_framerate(ranges[0]);
	} else {
		for (int i = len - 2; i >= 0; i--)
			if (fps > ranges[i]) {
				set_framerate(ranges[i]);
				return;
			}
		set_framerate(ranges[len - 1]);
	}
}

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

#define FMT_KEYBIND(fname, kb) FMT_OPTION(fname, (scanning_what() == (kb)) ? "(Press any key)" : kb_label(kb))
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
	enter_lobby(), update_inlobby(), show_levels(), update_playing(), update_pause(), restart_session(),
	exit_to_main(), resume();

static void maybe_save_config(MenuType), cleanup_lobby_list(MenuType), maybe_disconnect(MenuType),
	maybe_leave_lobby(MenuType);

#define GHOST .ghost = TRUE
#define NORETURN .noreturn = TRUE
static Menu MENUS[MEN_SIZE] = {
	[MEN_NULL] = {NORETURN},
	[MEN_ERROR] = {"Error", GHOST},
	[MEN_RESULTS] = {},
	[MEN_INTRO] = {NORETURN, .update = update_intro},
	[MEN_MAIN] = {"Mario Together", NORETURN, .enter = reset_credits},
	[MEN_LEVEL_SELECT] = {"Level Select", .enter = show_levels},
	[MEN_SINGLEPLAYER] = {"Singleplayer"},
	[MEN_MULTIPLAYER_NOTE] = {"Read This First!", GHOST, .enter = enter_multiplayer_note},
	[MEN_MULTIPLAYER] = {"Multiplayer"},
	[MEN_HOST_LOBBY] = {"Host Lobby", .enter = maybe_reset_lobby_name},
	[MEN_LOBBY_ID] = {"Join a Lobby"},
	[MEN_FIND_LOBBY]
	= {"Find Lobbies", .update = update_find_lobbies, .enter = list_lobbies, .leave = cleanup_lobby_list},
	[MEN_JOINING_LOBBY] = {"Please wait...", .update = update_joining_lobby, .back_sound = "disconnect",
		      .leave = maybe_disconnect, GHOST},
	[MEN_LOBBY] = {"Lobby", .back_sound = "disconnect", .enter = enter_lobby, .update = update_inlobby,
		      .leave = maybe_leave_lobby},
	[MEN_OPTIONS] = {"Options", .leave = maybe_save_config},
	[MEN_CONTROLS] = {"Controls", .leave = maybe_save_config},
	[MEN_INGAME_PLAYING] = {.from = MEN_INGAME_PAUSE, .update = update_playing},
	[MEN_INGAME_PAUSE] = {"Paused", .update = update_pause},
};

static const char* NO_LOBBIES_FOUND = "No lobbies found";
#define LEVEL_SELECT_OPTION "Level: %s", FORMAT(level), .enter = MEN_LEVEL_SELECT

#define EDIT(var) .edit = (var), .edit_size = sizeof(var)
#define TOGGLE(fname) .flip = toggle_##fname
#define FORMAT(fname) .format = fmt_##fname
#define REBIND(fname) .button = rebind_##fname
#define DISABLE .disabled = TRUE
#define VIVID .vivid = TRUE
#define OINFO DISABLE, VIVID
#define NOCLIENT .disable_if = is_client

static Option OPTIONS[MEN_SIZE][MAX_OPTIONS] = {
	[MEN_MAIN] = {
		{"Singleplayer", .enter = MEN_SINGLEPLAYER},
		{"Multiplayer", .enter = MEN_MULTIPLAYER_NOTE},
		{"Options", .enter = MEN_OPTIONS},
		{"Exit", .button = instaquit},
	},
	[MEN_SINGLEPLAYER] = {
		{LEVEL_SELECT_OPTION},
		{},
		{"Start!", .button = play_singleplayer},
	},
	[MEN_MULTIPLAYER_NOTE] = {
		{"Mario Together uses NutPunch to make peer-to-peer", OINFO},
		{"connections through UDP hole-punching.", OINFO},
		{"For a smooth experience, make sure that:", OINFO},
		{"  1. You and your friends' game is up to date.", OINFO},
		{"  2. Your VPN client is off.", OINFO},
		{"  3. Your firewall isn't blocking the game.", OINFO},
		{},
		{"If you are able to join lobbies AND see other players in", OINFO},
		{"them, then it should be working.", OINFO},
		{},
		{"OK", .button = read_multiplayer_note},
		{"Don't Show Again", .button = screw_multiplayer_note},
		{"NutPunch Troubleshooting (opens browser)", .button = open_troubleshooting_url},
	},
	[MEN_MULTIPLAYER] = {
		{"Find Lobbies", .enter = MEN_FIND_LOBBY},
		{"Host a Lobby", .enter = MEN_HOST_LOBBY},
		{"Enter a Lobby ID", .enter = MEN_LOBBY_ID},
	},
	[MEN_OPTIONS] = {
		{"Name: %s", FORMAT(name), EDIT(CLIENT.user.name)},
		{},
		{"Controls", .enter = MEN_CONTROLS},
		{"Input Delay: %i", FORMAT(delay), .flip = set_input_delay},
		{},
		{"Resolution: %dx%d", .disable_if = get_fullscreen, FORMAT(resolution), TOGGLE(resolution)},
		{"Fullscreen: %s", FORMAT(fullscreen), TOGGLE(fullscreen)},
		{"Framerate: %i FPS", FORMAT(framerate), .flip = flip_framerate},
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
		{LEVEL_SELECT_OPTION},
		{},
		{"Host!", .button = do_host_fr},
	},
	[MEN_LOBBY_ID] = {
		{"Lobby ID: %s", FORMAT(lobby), EDIT(CLIENT.lobby.name)},
		{},
		{"Join!", .button = do_join_fr},
	},
	[MEN_FIND_LOBBY] = {
		{NULL, DISABLE},
	},
	[MEN_JOINING_LOBBY] = {
		{"Joining lobby \"%s\"", DISABLE, FORMAT(active_lobby)},
	},
	[MEN_LOBBY] = {
		{"%s%s", DISABLE, FORMAT(active_lobby)},
		{},
		{"Players: %d", DISABLE, FORMAT(lobby_limit)},
		{LEVEL_SELECT_OPTION, NOCLIENT},
		{},
		{"Enter as: %s", FORMAT(spectate), .flip = set_spectate},
		{"%s", FORMAT(start), .disable_if = disable_start, .button = start_multiplayer},
	},
	[MEN_INGAME_PLAYING] = {},
	[MEN_INGAME_PAUSE] = {
		{"Resume Game", .button = resume},
		{"%s", FORMAT(restart), .button = restart_session},
		{"%s", FORMAT(exit), .button = exit_to_main},
	},
};

static void update_menu_track() {
	const char* track = NULL;
	switch (cur_menu) {
	default: {
		if (CLIENT.game.kevin && CLIENT.game.fred) {
			track = "it_makes_me_burn";
			break;
		}

		for (SecretType i = 0; i < SECR_SIZE; i++)
			if (SECRETS[i].active && SECRETS[i].track != NULL)
				track = SECRETS[i].track;

		if (track == NULL)
			track = "title";
		break;
	}

	case MEN_INTRO:
	case MEN_ERROR:
		break;

	case MEN_RESULTS: {
		if (!losers)
			track = "yi_score";
		break;
	}
	}

	if (track != NULL)
		play_generic_track(track, PLAY_LOOPING);
	else
		stop_generic_track();
}

static void reset_credits() {
	credits_y = SCREEN_HEIGHT;
}

static void join_found_lobby() {
	const NutPunch_LobbyInfo* lobby = NutPunch_GetLobby((int)MENUS[MEN_FIND_LOBBY].option);
	if (lobby == NULL)
		return;
	join_lobby(lobby->name);
	set_menu(MEN_JOINING_LOBBY);
}

void load_menu() {
	load_texture("ui/disclaimer", FALSE);
	load_texture("ui/background", FALSE);
	load_texture("ui/shortcut", FALSE);

	load_font("main", TRUE);

	load_sound("switch", FALSE);
	load_sound("select", TRUE);
	load_sound("toggle", FALSE);
	load_sound("enter", TRUE);
	load_sound("on", FALSE);
	load_sound("off", FALSE);
	load_sound("bump", FALSE);
	load_sound("connect", FALSE);
	load_sound("disconnect", TRUE);

	load_track("title", TRUE);
	load_track("yi_score", TRUE);

	load_secrets();
}

void menu_init() {
	load_menu();
	update_discord_status(NULL);
	from_scratch();
}

static void enter_multiplayer_note() {
	if (CLIENT.user.aware)
		set_menu(MEN_MULTIPLAYER);
}

static void maybe_save_config(MenuType next) {
	if (next != MEN_OPTIONS && next != MEN_CONTROLS) {
		CLIENT.lobby.name[0] = 0; // just in case the player changed their name
		maybe_reset_lobby_name();
		save_config();
	}
}

static void cleanup_lobby_list(MenuType next) {
	maybe_disconnect(next);
	for (int i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[MEN_FIND_LOBBY][i];
		opt->name = i ? NULL : NO_LOBBIES_FOUND;
		opt->disabled = TRUE, opt->button = NULL;
	}
}

static void update_intro() {
	if (totalticks() >= 150 || kb_pressed(KB_UI_ENTER))
		set_menu(MEN_MAIN);
}

#define MAX_LEN (64)
static void update_find_lobbies() {
	static char block[MAX_OPTIONS][MAX_LEN] = {0};
	for (int i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[MEN_FIND_LOBBY][i];
		if (NutPunch_LobbyCount() <= 0) {
			opt->name = i ? NULL : NO_LOBBIES_FOUND;
			opt->disabled = TRUE, opt->button = NULL;
		} else if (i >= NutPunch_LobbyCount()) {
			opt->name = NULL, opt->disabled = TRUE, opt->button = NULL;
		} else {
			const NutPunch_LobbyInfo* lobby = NutPunch_GetLobby(i);
			SDL_snprintf(block[i], MAX_LEN, "%s (%d/%d)", lobby->name, lobby->players, lobby->capacity);
			opt->name = block[i], opt->disabled = FALSE, opt->button = join_found_lobby;
		}
	}
}
#undef MAX_LEN

static void update_joining_lobby() {
	if (net_error()) {
		play_generic_sound("disconnect");
		show_error("Failed to %s lobby:\n\n%s", net_verb(), net_error());
	} else if (NutPunch_IsReady()) {
		push_user_data();
		(NutPunch_IsMaster() ? push_lobby_data : pull_lobby_data)();
		play_generic_sound("connect");
		set_menu(MEN_LOBBY);
	}
}

static void enter_lobby() {
	if (!NutPunch_IsReady()) {
		prev_menu();
		return;
	}

	update_discord_status(NULL);
}

static void maybe_leave_lobby(MenuType next) {
	if (next < MEN_INGAME && next != MEN_LEVEL_SELECT && next != MEN_RESULTS && next != MEN_ERROR)
		disconnect();
}

static void update_inlobby() {
	handle_chat_inputs();

	if (!NutPunch_IsReady()) {
		const char* error = net_error();
		if (error == NULL)
			prev_menu();
		else {
			show_error("Lost connection\n%s", net_error());
			disconnect();
		}
		play_generic_sound("disconnect");
		return;
	}

	if (!NutPunch_IsMaster())
		pull_lobby_data();
}

static void common_game_update() {
	if (game_exists() && !update_game())
		input_wipeout();
}

static void update_playing() {
	handle_chat_inputs();
	common_game_update();
}

static void update_pause() {
	common_game_update();
}

static void resume() {
	set_menu(MEN_INGAME_PLAYING);
}

void enter_gaming_menu() {
	set_menu(MEN_INGAME_PLAYING);
}

static void restart_session() {
	// FIXME: Update this if replays are implemented.

	if (NutPunch_IsReady())
		goto no_can_restart;

	if (game_state.flags & GF_HELL) {
		restart_game_session();
		return;
	}

	PlayerID can_restart = numplayers();
	for (PlayerID i = 0L; i < numplayers(); i++) {
		GamePlayer* player = get_player(i);
		if (player == NULL)
			continue;
		if (player->lives <= 0L)
			--can_restart;
		else if (game_state.sequence.type != SEQ_LOSE)
			--player->lives;
	}

	if (can_restart <= 0L) {
	no_can_restart:
		play_generic_sound("bump");
	}
}

static void exit_to_main() {
	extern Bool quickstart, permadeath;
	permadeath |= quickstart;
	nuke_game(), disconnect();
	set_menu(MENUS[MEN_INGAME_PLAYING].from);
}

static void maybe_disconnect(MenuType next) {
	if (next != MEN_LOBBY && next != MEN_JOINING_LOBBY)
		disconnect();
}

static const char* keve() {
	if (last_secret >= 0) {
		static char buf[MAX_SECRET_INPUTS + 1] = "";
		SDL_strlcpy(buf, SECRETS[last_secret].name, SECRETS[last_secret].state + 1);
		return buf;
	}

	if (CLIENT.game.kevin && CLIENT.game.fred)
		return "NOSTALIGA";

	const char* str = NULL;
	for (SecretType i = 0; i < SECR_SIZE; i++)
		if (SECRETS[i].active)
			str = SECRETS[i].name;
	return str;
}

static Bool is_option_disabled(const Option* opt) {
	if (opt->disable_if && opt->disable_if())
		return TRUE;
	return !opt->name || opt->disabled;
}

static const char* opt_name(const Option* opt) {
	return opt->format ? opt->format(opt->name) : opt->name;
}

static void select_menu_option_by_mouse(int* new_option) {
	float x = 0.f, y = 0.f;
	get_cursor_pos(&x, &y);

	static float last_y = 0.f;
	if (SDL_fabsf(last_y - y) < 1e-3)
		return; // don't change menu option unless the cursor moves vertically
	last_y = y;

	const int iopt = (int)SDL_floorf((y - 60.f) / 24.f);
	*new_option = MAX_OPTIONS;

	if (iopt < 0 || iopt >= MAX_OPTIONS || is_option_disabled(&OPTIONS[cur_menu][iopt]))
		return;

	const float margin = 8.f;
	x -= 48.f;

	const Option* opt = &OPTIONS[cur_menu][iopt];
	if (x < -margin || x > string_width("main", 24.f, opt_name(opt)) + margin)
		return;

	*new_option = iopt;
}

static void select_menu_option_by_keyboard(const int old_option, int* new_option, const int change) {
	if (!change)
		return;

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		if (change < 0 && *new_option <= 0)
			*new_option = MAX_OPTIONS - 1;
		else if (change > 0 && *new_option >= (MAX_OPTIONS - 1))
			*new_option = 0;
		else
			*new_option += change;

		Option* option = &OPTIONS[cur_menu][*new_option];
		if (!is_option_disabled(option))
			return;
	}

	*new_option = MAX_OPTIONS;
}

static void select_menu_option() {
	const int old_option = MENUS[cur_menu].option;
	const Bool select = kb_pressed(KB_UI_ENTER) || mb_pressed(SDL_BUTTON_LMASK);
	int new_option = old_option;

	if (is_option_disabled(&OPTIONS[cur_menu][new_option])) {
		// jump to first enabled option, if any
		new_option = -1, select_menu_option_by_keyboard(old_option, &new_option, 1);
	} else {
		const int change = kb_repeated(KB_UI_DOWN) - kb_repeated(KB_UI_UP);
		select_menu_option_by_keyboard(old_option, &new_option, change);
		select_menu_option_by_mouse(&new_option);
	}

	if (new_option == MAX_OPTIONS)
		return;

	if (old_option != new_option) {
		MENUS[cur_menu].option = new_option;
		play_generic_sound("switch");
	}

	if (select) {
		Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
		if (is_option_disabled(opt))
			goto NO_SELECT_CYKA;

		play_generic_sound("select");
		if (opt->button)
			opt->button();
		else if (opt->flip)
			opt->flip(1);
		else if (opt->edit)
			start_typing(opt->edit, opt->edit_size); // FIXME: Play "select" when pressing Enter or
			                                         //        backspace while typing
		else if (opt->enter)
			set_menu(opt->enter);
	}

NO_SELECT_CYKA:
	int flip = kb_repeated(KB_UI_RIGHT) - kb_repeated(KB_UI_LEFT);
	if (!flip)
		return;

	Option* opt = &OPTIONS[cur_menu][MENUS[cur_menu].option];
	if (opt->flip && !is_option_disabled(opt)) {
		play_generic_sound("toggle");
		opt->flip(flip);
	}
}

void update_menu() {
	float ahead = 0.f;
	if (cur_menu == MEN_INGAME_PLAYING) {
		extern GekkoSession* game_session;
		gekko_network_poll(game_session);
		ahead = gekko_frames_ahead(game_session);
		ahead = SDL_clamp(ahead, 0, 2);
	}

	for (new_frame(ahead); got_ticks(); next_tick()) {
		int last_menu = cur_menu;
		if (MENUS[cur_menu].update != NULL)
			MENUS[cur_menu].update();
		if (last_menu != cur_menu)
			continue;

		update_secrets();
		select_menu_option();

		if (kb_pressed(KB_PAUSE)) {
			const char* sound = MENUS[cur_menu].back_sound;
			if (!sound)
				sound = "select";
			if (prev_menu())
				play_generic_sound(sound);
		}
	}

	extern void interpolate();
	interpolate();
}

void draw_menu() {
	start_drawing();

	if (cur_menu == MEN_INTRO)
		goto draw_intro;

	batch_reset();
	clear_color(0.f, 0.f, 0.f, 1.f);

	batch_sprite("ui/background");

	if (cur_menu >= MEN_INGAME)
		draw_game();

	if (cur_menu == MEN_INGAME_PAUSE) {
		batch_pos(B_XYZ(0.f, 0.f, -10000.f)), batch_color(B_RGBA(0, 0, 0, 128));
		batch_rectangle(NULL, B_XY(SCREEN_WIDTH, SCREEN_HEIGHT));
	}

	Bool has_secret = FALSE;
	for (SecretType i = 0; i < SECR_SIZE; i++)
		if (SECRETS[i].active) {
			has_secret = TRUE;
			break;
		}

	const float lk = 0.125f * dt();
	secret_lerp = glm_lerp(secret_lerp, has_secret, SDL_min(lk, 1.f));

	const Uint8 fade_red = (CLIENT.game.kevin && CLIENT.game.fred) ? 255 : 32;
	const Uint8 fade_alpha = (Uint8)(secret_lerp * 255.f);
	batch_pos(B_XY(-SCREEN_WIDTH, 0));
	batch_colors((Uint8[4][4]){
		{fade_red, 0, 0, 0         },
                {fade_red, 0, 0, 0         },
                {0,        0, 0, fade_alpha},
                {0,        0, 0, fade_alpha}
        });
	batch_rectangle(NULL, B_XY(SCREEN_WIDTH * 3, SCREEN_HEIGHT));
	batch_color(B_WHITE);

	Menu* menu = &MENUS[cur_menu];
	const float menu_y = 60.f;

	const float lx = 0.6f * dt();
	menu->cursor = glm_lerp(menu->cursor, (float)menu->option, SDL_min(lx, 1.f));
	if (!is_option_disabled(&OPTIONS[cur_menu][menu->option])) {
		batch_pos(B_XY(44.f + (SDL_sinf(totalticks() / 5.f) * 4.f), menu_y + (menu->cursor * 24.f)));
		batch_align(B_TOP_RIGHT);
		batch_string("main", 24.f, ">");
	}

	batch_pos(B_XY(48.f, 24.f)), batch_color(B_RGBA(255, 144, 144, 192)), batch_align(B_TOP_LEFT);
	batch_string("main", 24.f, menu->name);

	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[cur_menu][i];
		if (!opt->name)
			continue;

		const float lx = 0.5f * dt();
		opt->hover = glm_lerp(
			opt->hover, (float)(menu->option == i && !is_option_disabled(opt)), SDL_min(lx, 1.f));

		const char *suffix = "", *format = opt_name(opt);
		if (opt->edit != NULL && typing_what() == opt->edit && SDL_fmodf(totalticks(), 30.f) < 16.f)
			suffix = "|";

		static char name[512] = {0}; // separate fmt buffer to prevent memory-related bugs
		SDL_snprintf(name, sizeof(name), "%s%s", format, suffix);

		const float x = 48.f + (opt->hover * 8.f);
		const float y = menu_y + ((float)i * 24.f);

		batch_pos(B_XY(x, y)), batch_color(B_ALPHA(is_option_disabled(opt) && !opt->vivid ? 128 : 255));
		batch_align(B_TOP_LEFT);
		batch_string("main", 24.f, name);

		if (opt->enter != MEN_NULL) {
			batch_pos(B_XY(x + string_width("main", 24.f, name) + 4.f, y + 8.f));
			batch_sprite("ui/shortcut");
		}
	}

	if (menu->from != MEN_NULL && cur_menu < MEN_INGAME) {
		const char* btn = "Back";
		if (MENUS[cur_menu].from == MEN_EXIT)
			btn = "Exit";
		else if (cur_menu == MEN_JOINING_LOBBY || cur_menu == MEN_LOBBY)
			btn = "Disconnect";
		const char* indicator = fmt("[%s] %s", kb_label(KB_PAUSE), btn);
		batch_pos(B_XY(48.f, SCREEN_HEIGHT - 24.f)), batch_color(B_ALPHA(128)), batch_align(B_BOTTOM_LEFT);
		batch_string("main", 24.f, indicator);
	}

	switch (cur_menu) {
	default:
		break;

	case MEN_MAIN: {
		credits_y -= dt() * 0.5f;
		if (credits_y <= ((float)entries(credits) * -18.f))
			credits_y = SCREEN_HEIGHT;

		for (int i = 0; i < entries(credits); i++) {
			const float y = credits_y + ((float)i * 18.f);
			if (y >= (SCREEN_HEIGHT - 18.f) || y <= 0.f)
				continue;

			const float top = 200.f, bottom = SCREEN_HEIGHT - 200.f;
			if ((y + 18.f) > bottom)
				batch_color(B_ALPHA(128 - (Uint8)(((y + 18.f - bottom) / 200.f) * 128.f)));
			else if (y < top)
				batch_color(B_ALPHA(128 - (Uint8)(((top - y) / 200.f) * 128.f)));
			else
				batch_color(B_ALPHA(128));

			batch_pos(B_XY(SCREEN_WIDTH - 48.f, y)), batch_align(B_TOP_RIGHT);
			batch_string("main", 18.f, credits[i]);
		}
		break;
	}

	case MEN_FIND_LOBBY: {
		batch_pos(B_XY(SCREEN_WIDTH - 48.f, 24.f)), batch_color(B_ALPHA(128)), batch_align(B_TOP_RIGHT);
		batch_string("main", 24.f, fmt("Server: %s", get_hostname()));
		break;
	}

	case MEN_LOBBY: {
		batch_pos(B_XY(SCREEN_WIDTH - 48.f, 24.f)), batch_color(B_RGBA(255, 144, 144, 192));
		batch_align(B_TOP_RIGHT);
		const int num_peers = NutPunch_PeerCount();
		batch_string("main", 24.f, fmt("Players (%i / %i)", num_peers, NutPunch_GetMaxPlayers()));

		batch_color(B_ALPHA(128));
		float y = 60.f;
		int idx = 1;
		for (int i = 0; i < MAX_PEERS; i++) {
			if (!NutPunch_PeerAlive(i))
				continue;
			batch_pos(B_XY(SCREEN_WIDTH - 48.f, y)), batch_align(B_TOP_RIGHT);
			batch_string("main", 24.f,
				fmt("%i. %s%s", idx, get_peer_name(i), peer_is_spectating(i) ? " (SPEC)" : ""));
			++idx;
			y += 24.f;
		}

		break;
	}

	case MEN_RESULTS: {
		batch_pos(B_XY(HALF_SCREEN_WIDTH, 174.f)), batch_color(B_RGBA(255, 144, 144, 192));
		batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_string("main", 24.f, "Scoreboard");

		for (PlayerID i = 0; i < MAX_PLAYERS; i++) {
			if (winners[i].name[0] == '\0')
				continue;
			batch_pos(B_XY(HALF_SCREEN_WIDTH, 210.f + (i * 24.f)));
			batch_color((i == 0) ? B_RGBA(255, 255, 0, 192) : ((i >= 3) ? B_ALPHA(128) : B_ALPHA(192)));
			batch_string("main", 24.f,
				fmt("%i. %s x%i, %i pts", i + 1, winners[i].name, winners[i].lives, winners[i].score));
		}

		break;
	}
	}

	draw_chat();

	batch_pos(B_XY(HALF_SCREEN_WIDTH + ((fade_red >= 255) ? (SDL_rand(3) - SDL_rand(3)) : 0.f),
		SCREEN_HEIGHT - 48.f - ((fade_red >= 255) ? SDL_rand(5) : 0.f)));
	batch_color(B_RGB(255, 96, 96));
	batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
	const float kscale = 1.f + ((0.84f + SDL_sinf(totalticks() / 32.f) * 0.16f) * secret_lerp);
	batch_scale(B_SCALE(kscale));
	batch_string("main", 24.f, keve());

	if (cur_menu != MEN_JOINING_LOBBY && cur_menu != MEN_LOBBY) {
		batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 48.f)), batch_scale(B_SCALE(1.f));
		batch_color(B_ALPHA(128 * secret_lerp)), batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_string("main", 24, fmt("[%s] Deactivate", kb_label(KB_SECRET_BAIL)));
	}

	goto jobwelldone;

draw_intro:
	clear_color(0.f, 0.f, 0.f, 1.f);

	float a = 1.f, ticks = totalticks();
	if (ticks < 25.f)
		a = ticks / 25.f;
	if (ticks > 125.f)
		a = 1.f - ((ticks - 100.f) / 25.f);

	batch_alpha_test(0.f);
	batch_pos(B_XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT)), batch_color(B_ALPHA(a * 255));
	batch_sprite("ui/disclaimer");
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
		OPTIONS[MEN_ERROR][k].disabled = TRUE;
	}
	while (line) {
		OPTIONS[MEN_ERROR][linum++].name = line;
		line = SDL_strtok_r(NULL, sep, &state);
	}

	set_menu(MEN_ERROR);

	if (quickstart)
		MENUS[MEN_ERROR].from = MEN_EXIT;
}

void populate_results() {
	SDL_zero(winners);
	for (PlayerID i = 0; i < numplayers(); i++) {
		const char* name = get_peer_name(player_to_peer(i));
		SDL_strlcpy(winners[i].name, (name == NULL) ? fmt("Player %i", i + 1) : name, sizeof(winners[i].name));
	}
}

void show_results() {
	for (int i = 0; i < MAX_OPTIONS; i++) {
		OPTIONS[MEN_RESULTS][i].name = "";
		OPTIONS[MEN_RESULTS][i].disabled = TRUE;
	}

	char idx = 0;

	// Endgame reason
	losers = TRUE;
	for (PlayerID i = 0; i < numplayers(); i++) {
		const GamePlayer* player = get_player(i);
		if (player->lives >= 0L) {
			losers = FALSE;
			break;
		}
	}

	MENUS[MEN_RESULTS].name = losers ? "Game Over" : "World Completed";
	OPTIONS[MEN_RESULTS][idx++].name
		= losers ? "No players remaining"
	                 : ((game_state.flags & GF_KEVIN) ? ((game_state.flags & GF_FRED) ? "They will be back..."
											  : "Kevin will be back...")
							  : ((game_state.flags & GF_FRED) ? "Fred will be back..."
											  : "All levels cleared"));

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
			cmp[0] = winners[j], cmp[1] = winners[j + 1];
			winners[j] = cmp[1], winners[j + 1] = cmp[0];
		}

	set_menu(MEN_RESULTS);
}

const char* who_is_winner(int idx) {
	return (idx < 0 || idx >= entries(winners)) ? NULL : winners[idx].name;
}

Bool set_menu(MenuType next_menu) {
	if (next_menu == MEN_EXIT) {
		permadeath = TRUE;
		return TRUE;
	}

	if (cur_menu == next_menu || next_menu <= MEN_NULL || next_menu >= MEN_SIZE)
		return FALSE;

	// HACK: extra-special ingame-menu handling.
	if ((cur_menu == MEN_INGAME_PLAYING && next_menu == MEN_INGAME_PAUSE)
		|| (cur_menu == MEN_INGAME_PAUSE && next_menu == MEN_INGAME_PLAYING))
	{
		MENUS[MEN_INGAME_PAUSE].from = MEN_INGAME_PLAYING;
		cur_menu = next_menu;

		extern void pause_gamestate(), unpause_gamestate();
		if (next_menu == MEN_INGAME_PLAYING)
			unpause_gamestate();
		else
			pause_gamestate();

		return TRUE;
	}

	if (next_menu == MEN_RESULTS) // HACK: ghosting doesn't return to main menu from results???
		MENUS[MEN_RESULTS].from = MENUS[MEN_INGAME_PLAYING].from;
	else if (cur_menu < MEN_INGAME && MENUS[cur_menu].from != next_menu) {
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
	int new_option = MENUS[cur_menu].option;
	for (size_t i = 0; i < MAX_OPTIONS; i++) {
		Option* option = &OPTIONS[cur_menu][new_option];
		if (!is_option_disabled(option)) {
			MENUS[cur_menu].option = new_option;
			MENUS[cur_menu].cursor = (float)new_option;
			option->hover = 1;
			break;
		}
		new_option = (new_option + 1) % MAX_OPTIONS;
	}

	update_menu_track();
	return TRUE;
}

Bool prev_menu() {
	if (cur_menu == MEN_INGAME_PLAYING)
		return set_menu(MEN_INGAME_PAUSE);
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
	} else if (volume > 0.01f && (volume + delta) < 0.f) {
		volume = 0.f;
	} else if (volume <= 0.01f && (volume + delta) <= 0.f) {
		volume = 1.f;
	} else {
		volume += delta;
	}

	return volume;
}

// Level-select junk

static void select_level() {
	const int m = MEN_LEVEL_SELECT;
	const char* name = OPTIONS[m][(int)MENUS[m].option].name;
	SDL_strlcpy(CLIENT.game.level, name, CLIENT_STRING_MAX);
	if (NutPunch_IsMaster())
		push_lobby_data();
	prev_menu();
}

#define LEVELS_PER_PAGE (10)
static int levels_page = 0;

static void levels_next_page() {
	levels_page++;
	show_levels();
}

static void levels_prev_page() {
	levels_page -= 1;
	show_levels();
}

static void show_levels() {
	static char block[MAX_OPTIONS][CLIENT_STRING_MAX] = {0};

	MENUS[MEN_LEVEL_SELECT].option = 0;
	for (int i = 0; i < MAX_OPTIONS; i++) {
		Option* opt = &OPTIONS[MEN_LEVEL_SELECT][i];
		opt->disabled = TRUE, opt->name = NULL, opt->button = NULL;
	}

	int count = 0;
	char** glob = SDL_GlobDirectory(fmt("%s%s", get_data_path(), "data/levels"), NULL, 0, &count);

	Option* pager = &OPTIONS[MEN_LEVEL_SELECT][LEVELS_PER_PAGE + 1];
	if (levels_page < (count - 1) / LEVELS_PER_PAGE) {
		pager->name = "Next ->", pager->disabled = FALSE;
		pager->button = levels_next_page, pager += 1;
	}

	if (levels_page > 0) {
		pager->name = "<- Back", pager->disabled = FALSE;
		pager->button = levels_prev_page;
	}

	const int off = levels_page * LEVELS_PER_PAGE;
	for (int i = 0; i < count - off && i < LEVELS_PER_PAGE; i++) {
		const char* level = filename_sans_extension(glob[off + i]);
		SDL_strlcpy(block[i], level, CLIENT_STRING_MAX);
		Option* opt = &OPTIONS[MEN_LEVEL_SELECT][i];
		opt->disabled = FALSE, opt->name = block[i];
		opt->button = select_level;
	}

	SDL_free((void*)glob);
}

#undef LEVELS_PER_PAGE
