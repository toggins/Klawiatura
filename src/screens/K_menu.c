#include <SDL3/SDL_platform_defines.h>

#include "K_audio.h"
#include "K_cmake.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_net.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "uis/K_message.h"

enum {
    MEN_NULL,

    MEN_MAIN,
    MEN_SINGLEPLAYER,
    MEN_MULTIPLAYER,
    MEN_REPLAYS,
    MEN_HOST_LOBBY,
    MEN_LOBBY_LIST,
    MEN_LOBBY,
    MEN_EDITOR,

    MEN_SIZE,
};

static const char* credits[8][2] = {
    {"menu.credits.mario_forever",  "menu.credits.mario_forever.text" },
    {"menu.credits.graphics",       "menu.credits.graphics.text"      },
    {"menu.credits.audio",          "menu.credits.audio.text"         },
    {"menu.credits.programming",    "menu.credits.programming.text"   },
    {"menu.credits.levels",         "menu.credits.levels.text"        },
    {"menu.credits.beta_testing",   "menu.credits.beta_testing.text"  },
    {"menu.credits.special_thanks", "menu.credits.special_thanks.text"},
    {NULL,                          "menu.credits.end"                },
};

static const char* replay_error = NULL;

static void enter_replays_menu(MenuType), leave_replays_menu(MenuType), enter_lobby_list_menu(MenuType),
    tick_lobby_list_menu(), enter_lobby_menu(MenuType), leave_lobby_menu(MenuType), tick_lobby_menu();
static Bool draw_main_menu(), draw_replays_menu(), draw_lobby_list_menu(), draw_lobby_menu(), kick_player_disabled(),
    start_disabled(), character_disabled();

static const char *fmt_max_peers(size_t), *fmt_visibility(size_t), *fmt_lobby(), *fmt_character(size_t),
    *fmt_powerup(size_t), *fmt_world(size_t), *fmt_test_level(size_t);
static void multiplayer_option(), options_option(), exit_option(), max_peers_cycle(Sint8), visibility_cycle(Sint8),
    host_option(), character_cycle(Sint8), powerup_cycle(Sint8), enter_as_cycle(Sint8), kick_player_option(),
    world_cycle(Sint8), start_option(), go_to_editor_option(), test_level_cycle(Sint8), test_level_option();

static Catalog CATALOG = {
	.current = MEN_MAIN,

	.menus = {
        [MEN_MAIN] = {
            .name = "title",
            .draw = draw_main_menu,
            .width =
#ifdef SDL_PLATFORM_EMSCRIPTEN
            2,
#else
            3,
#endif
        },

		[MEN_SINGLEPLAYER] = {
			.name = "option.singleplayer",
		},

		[MEN_MULTIPLAYER] = {
			.name = "option.multiplayer",
		},

        [MEN_REPLAYS] = {
            .name = "option.replays",
            .enter = enter_replays_menu,
            .leave = leave_replays_menu,
            .draw = draw_replays_menu,
        },

		[MEN_HOST_LOBBY] = {
			.name = "option.host_lobby",
		},

		[MEN_LOBBY_LIST] = {
			.name = "option.find_lobby",
			.enter = enter_lobby_list_menu,
			.tick = tick_lobby_list_menu,
            .draw = draw_lobby_list_menu,
		},

        [MEN_LOBBY] = {
			.fmt = fmt_lobby,
			.leave = leave_lobby_menu,
			.tick = tick_lobby_menu,
			.draw = draw_lobby_menu,
		},

        [MEN_EDITOR] = {
			.name = "option.editor",
        }
	},

	.options = {
		[MEN_MAIN] = {
			{.name = "option.singleplayer", .menu = MEN_SINGLEPLAYER},
			{.name = "option.multiplayer", .callback = multiplayer_option},
#ifdef SDL_PLATFORM_EMSCRIPTEN
            {.name = "option.replays", .menu = MEN_REPLAYS},
            {.name = "option.options", .callback = options_option},
#else
            {.name = "option.options", .callback = options_option},
            {.name = "option.replays", .menu = MEN_REPLAYS},
            {.name = "option.editor", .menu = MEN_EDITOR},
			{.name = "option.exit", .callback = exit_option},
#endif
		},

        [MEN_SINGLEPLAYER] = {
            {.fmt = fmt_world, .cycle = world_cycle},
            {},
            {.fmt = fmt_character, .cycle = character_cycle},
            {.fmt = fmt_powerup, .cycle = powerup_cycle},
            {},
            {.name = "option.start", .disabled = start_disabled, .callback = start_option},
        },

		[MEN_MULTIPLAYER] = {
			{.name = "option.host_lobby", .menu = MEN_HOST_LOBBY},
			{.name = "option.find_lobby", .menu = MEN_LOBBY_LIST},
		},

		[MEN_HOST_LOBBY] = {
			{.fmt = fmt_max_peers, .cycle = max_peers_cycle},
			{.fmt = fmt_visibility, .cycle = visibility_cycle},
			{},
			{.name = "option.host", .callback = host_option},
		},

        [MEN_LOBBY] = {
            {.name = "option.world", .disabled = is_client, .cycle = world_cycle},
            {.name = "option.enter_as", .cycle = enter_as_cycle},
            {.name = "option.characer", .disabled = character_disabled, .cycle = character_cycle},
            {.name = "option.powerup", .disabled = character_disabled, .cycle = powerup_cycle},
            {.name = "option.options", .callback = options_option},
            {.name = "option.kick", .disabled = kick_player_disabled, .callback = kick_player_option},
            {.name = "option.start", .disabled = start_disabled, .callback = start_option},
        },

        [MEN_EDITOR] = {
            {.name = "option.go_to_editor", .callback = go_to_editor_option},
            {.fmt = fmt_test_level, .cycle = test_level_cycle, .callback = test_level_option},
            {},
            {.name = "option.open_data_folder", .callback = open_data_folder},
        },
	}
};

// =====
// MENUS
// =====

static void draw_main_button(size_t idx, const char* button, const char* icon, const char* name, const float pos[2]) {
    batch_reset();

    batch_pos(B_F3_XY(pos[0], pos[1]));
    batch_sprite(button);
    if (CATALOG.menus[MEN_MAIN].option == idx) {
        batch_blend(BM_ADD);
        batch_color(B_U4_VALUE(96.f + (SDL_sinf(screenticks() * 0.15f) * 32.f)));
        batch_sprite(button);
        batch_color(B_U4_WHITE);
        batch_blend(BM_NORMAL);
    }

    batch_pos(B_F3_XY(pos[0] - 44.f, pos[1] - 40.f));
    batch_sprite(icon);

    batch_pos(B_F3_XY(pos[0], pos[1] + 57.f));
    batch_align(B_ALIGN_CENTER);
    batch_string("menu", 19.f, LFMT(name));
    batch_align(B_ALIGN_TOP_LEFT);
}

static Bool draw_main_menu() {
    batch_reset();
    batch_pos(B_F3_XY(96.f, 130.f));
    batch_string("footer", 16.f, GAME_NAME " " GAME_VERSION);
    batch_pos(B_F3_XY(96.f, 146.f));
    batch_color(B_U4_ALPHA(160));
    batch_string("footer", 12.f, GAME_BUILD_DATE);
    batch_pos(B_F3_XY(SCREEN_WIDTH - 96.f, 130.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN_TOP_RIGHT);
    batch_string("footer", 16.f, fmt("Checksum: %u", get_game_hash()));

    float wrap = SCREEN_WIDTH;
    for (size_t i = 0; i < SDL_arraysize(credits); i++) {
        wrap += string_width("footer", 16.f, LFMT(credits[i][1]));
        if (i < (SDL_arraysize(credits) - 1))
            wrap += 32.f;
    }
    const float scroll = SDL_fmodf(screenticks(), wrap);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    draw_main_button(0, "ui/menu/buttons/singleplayer", "ui/menu/icons/game", "option.singleplayer",
        B_F2(HALF_SCREEN_WIDTH - 84.f, 222.f));
    draw_main_button(1, "ui/menu/buttons/multiplayer", "ui/menu/icons/game", "option.multiplayer",
        B_F2(HALF_SCREEN_WIDTH + 84.f, 222.f));
    draw_main_button(
        2, "ui/menu/buttons/replays", "ui/menu/icons/game", "option.replays", B_F2(HALF_SCREEN_WIDTH - 84.f, 356.f));
    draw_main_button(3, "ui/menu/buttons/options", NULL, "option.options", B_F2(HALF_SCREEN_WIDTH + 84.f, 356.f));
#else
    draw_main_button(0, "ui/menu/buttons/singleplayer", "ui/menu/icons/game", "option.singleplayer",
        B_F2(HALF_SCREEN_WIDTH - 168.f, 222.f));
    draw_main_button(
        1, "ui/menu/buttons/multiplayer", "ui/menu/icons/game", "option.multiplayer", B_F2(HALF_SCREEN_WIDTH, 222.f));
    draw_main_button(2, "ui/menu/buttons/options", NULL, "option.options", B_F2(HALF_SCREEN_WIDTH + 168.f, 222.f));
    draw_main_button(
        3, "ui/menu/buttons/replays", "ui/menu/icons/game", "option.replays", B_F2(HALF_SCREEN_WIDTH - 168.f, 356.f));
    draw_main_button(
        4, "ui/menu/buttons/editor", "ui/menu/icons/editor", "option.editor", B_F2(HALF_SCREEN_WIDTH, 356.f));
    draw_main_button(5, "ui/menu/buttons/exit", NULL, "option.exit", B_F2(HALF_SCREEN_WIDTH + 168.f, 356.f));
#endif

    batch_color(
        B_U4_ALPHA(((scroll < 64.f) ? (scroll / 64.f)
                                    : ((scroll > (wrap - 64.f)) ? (1.f - ((scroll - (wrap - 64.f)) / 64.f)) : 1.f))
                   * 255.f));

    float cx = SCREEN_WIDTH - scroll;
    for (size_t i = 0; i < SDL_arraysize(credits); i++) {
        batch_pos(B_F3_XY(cx, SCREEN_HEIGHT - 24.f));
        batch_align(B_ALIGN_BOTTOM_LEFT);
        batch_string("footer", 16.f, LFMT(credits[i][0]));
        batch_align(B_ALIGN_TOP_LEFT);
        const char* text = LFMT(credits[i][1]);
        batch_string("footer", 16.f, text);
        cx += string_width("footer", 16.f, text) + 32.f;
    }

    return FALSE;
}

static LobbyListState lobby_list_last_state = LLS_READY;
static Uint16 lobby_list_refresh = 0;
static Bool lobby_list_hint = FALSE;

static const char* fmt_lobby_list(size_t idx) {
    const LobbyInfo* lobby = get_lobby_list(idx);
    return (lobby == NULL) ? NULL : fmt("%s (%u/%u)", lobby->name, lobby->peers, lobby->capacity);
}

static void lobby_option();
static Bool update_lobby_list() {
    SDL_zeroa(CATALOG.options[MEN_LOBBY_LIST]);

    if (get_lobby_list_state() == LLS_SEARCHING) {
        CATALOG.options[MEN_LOBBY_LIST][0].name = "option.finding_lobbies";
        CATALOG.options[MEN_LOBBY_LIST][0].disabled = always_disabled;

        return FALSE;
    }

    if (get_lobby_list_count() <= 0) {
        CATALOG.options[MEN_LOBBY_LIST][0].name = "option.no_lobbies";
        CATALOG.options[MEN_LOBBY_LIST][0].disabled = always_disabled;

        lobby_list_hint = TRUE;
        return FALSE;
    }

    for (size_t i = 0; i < MAX_OPTIONS; i++) {
        const LobbyInfo* lobby = get_lobby_list(i);
        if (lobby == NULL)
            break;

        CATALOG.options[MEN_LOBBY_LIST][i].fmt = fmt_lobby_list;
        CATALOG.options[MEN_LOBBY_LIST][i].callback = lobby_option;
    }

    lobby_list_hint = FALSE;
    return TRUE;
}

static void enter_lobby_list_menu(MenuType from) {
    (void)from;

    find_lobbies();
    update_lobby_list();
    lobby_list_last_state = get_lobby_list_state();
    lobby_list_hint = FALSE;
}

static void tick_lobby_list_menu() {
    const LobbyListState new_state = get_lobby_list_state();
    if (lobby_list_last_state != new_state) {
        lobby_list_refresh = (update_lobby_list() ? 24 : 8) * get_tickrate();
        lobby_list_last_state = new_state;
    }

    if (lobby_list_last_state == LLS_READY && lobby_list_refresh > 0) {
        if (--lobby_list_refresh <= 0) {
            find_lobbies();
            update_lobby_list();
            lobby_list_last_state = get_lobby_list_state();

            return;
        }
    }
}

static Bool draw_lobby_list_menu() {
    if (!lobby_list_hint)
        return TRUE;

    batch_reset();
    batch_pos(B_F3_HALF_SCREEN);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string_wrap("footer", 16.f, LFMT("option.check_checksum"), SCREEN_WIDTH - 32.f);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string_wrap("footer", 16.f, LFMT("option.your_checksum", 'u', get_game_hash()), SCREEN_WIDTH - 32.f);

    return TRUE;
}

static void replay_option();
static void iterate_replay_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    (void)buffer;
    (void)size;

    size_t* idx = userdata;
    if (*idx >= MAX_OPTIONS)
        return;

    Option* option = &CATALOG.options[MEN_REPLAYS][*idx];
    option->name = SDL_strdup(filename_no_ext(file_basename(filename)));
    option->callback = replay_option;

    ++*idx;
}

static void enter_replays_menu(MenuType from) {
    (void)from;

    leave_replays_menu(MEN_NULL);
    iterate_user_files("replays/*.rpl", FALSE, iterate_replay_file, &(size_t){0});
}

static void leave_replays_menu(MenuType to) {
    (void)to;

    for (size_t i = 0; i < MAX_OPTIONS; i++) {
        Option* option = &CATALOG.options[MEN_REPLAYS][i];
        SDL_free((void*)option->name);
        SDL_zerop(option);
    }
}

static Bool draw_replays_menu() {
    if (CATALOG.options[MEN_REPLAYS][0].name != NULL)
        return TRUE;

    batch_reset();
    batch_pos(B_F3_HALF_SCREEN);
    batch_colors(B_U4X4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("header", 32.f, LFMT("option.no_replays"));
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("option.how_to_record", 's', kb_label(KB_RECORD_REPLAY)));

    return TRUE;
}

static const char* fmt_lobby() {
    const char* lname = get_lobby_name();
    return fmt("%s (%s)", (lname != NULL && SDL_strnlen(lname, 33) > 32) ? fmt("%.*s...", 32, lname) : lname,
        LFMT(in_private_lobby() ? "value.private" : "value.public"));
}

static void leave_lobby_menu(MenuType to) {
    (void)to;

    disconnect();
    play_generic_sound("ui/disconnect", PLAY_SYSTEM);
}

static const char* fmt_disconnected() {
    const char* error = net_error();
    return (error == NULL) ? LFMT("message.disconnected") : fmt("%s\n(%s)", LFMT("message.disconnected"), error);
}

static void cancel_error() {
    previous_menu(&CATALOG);
}

static void tick_lobby_menu() {
    if (is_connected())
        return;

    UI* message = create_ui(UI_MESSAGE, NULL);
    if (message == NULL) {
        cancel_error();
    } else {
        UIMessageData* userdata = message->userdata;
        userdata->title = "message.error";
        userdata->fmt = fmt_disconnected;
        userdata->cancel = cancel_error;
    }
}

static void draw_lobby_cycle(float* y, size_t idx, const char* label, const char* value, Bool disabled) {
    const float y1 = *y;

    batch_pos(B_F3_XY(125.f, *y));
    batch_color(B_U4_ALPHA(disabled ? 160 : 255));
    batch_sprite(LFMT(label));

    *y += 24.f;
    batch_pos(B_F3_XY(125.f, *y));
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string_wrap("footer", 16.f, value, 186.f);

    if (!disabled) {
        const float hw = string_width_wrap("footer", 16.f, value, 186.f) * 0.5f;
        batch_pos(B_F3_XY(115.f - hw, *y));
        batch_flip(B_B2(TRUE, FALSE));
        batch_sprite("ui/menu/lobby/arrow");
        batch_pos(B_F3_XY(135.f + hw, *y));
        batch_flip(B_B2_FALSE);
        batch_sprite("ui/menu/lobby/arrow");
    }

    *y += string_height_wrap("footer", 16.f, value, 186.f) + 8.f;

    if (CATALOG.menus[MEN_LOBBY].option == idx && !disabled) {
        batch_pos(B_F3_XY(-240.f, y1 - 4.f));
        batch_colors(B_U4X4({0, 0, 0, 255}, {40, 40, 40, 255}, {0, 0, 0, 255}, {40, 40, 40, 255}));
        batch_blend(BM_ADD);
        batch_rectangle(NULL, B_F2(490.f, *y - y1));
        batch_blend(BM_NORMAL);
    }

    *y += 4.f;
}

static const char* fmt_lobby_world(size_t idx) {
    (void)idx;

    const World* world = get_world(is_connected() ? get_lobby_string("world") : CLIENT.world);
    return (world == NULL) ? NULL : LFMT(fmt("world.%s", world->name));
}

static const char* fmt_lobby_powerup(size_t idx) {
    (void)idx;

    const Sint8 cost = get_powerup_cost(CLIENT.powerup);
    return fmt("%s%s", get_powerup_name(CLIENT.powerup), (cost > 0) ? fmt(" (-%i)", cost) : "");
}

static void draw_lobby_button(float* y, size_t idx, const char* sprite, Bool disabled) {
    batch_pos(B_F3_XY(125.f, *y));
    batch_color(B_U4_ALPHA(disabled ? 128 : 255));
    batch_sprite(sprite);

    if (CATALOG.menus[MEN_LOBBY].option == idx && !disabled) {
        batch_blend(BM_ADD);
        batch_color(B_U4_VALUE(80));
        batch_sprite(sprite);
        batch_blend(BM_NORMAL);
    }

    *y += 48.f;
}

static Bool draw_lobby_menu() {
    batch_reset();

    // LEFT
    batch_pos(B_F3_XY(-240.f, 11.f));
    batch_colors(B_U4X4({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 128}, {0, 0, 0, 128}));
    batch_rectangle(NULL, B_F2(490.f, 2.f));
    batch_pos(B_F3_XY(-240.f, 13.f));
    batch_colors(B_U4X4({115, 156, 115, 255}, {115, 156, 115, 255}, {189, 231, 189, 255}, {189, 231, 189, 255}));
    const char* lname = fmt("%s (%s)", get_lobby_name(), LFMT(in_private_lobby() ? "value.private" : "value.public"));
    const float lh = string_height_wrap("footer", 16.f, lname, 218.f);
    batch_rectangle(NULL, B_F2(490.f, 6.f + lh));
    batch_pos(B_F3_XY(-240.f, 19.f + lh));
    batch_colors(B_U4X4({0, 0, 0, 128}, {0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 0}));
    batch_rectangle(NULL, B_F2(490.f, 2.f));
    batch_pos(B_F3_XY(125.f, 16.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string_wrap("footer", 16.f, lname, 218.f);

    float ly = lh + 30.f;

    draw_lobby_cycle(&ly, 0, "menu.lobby.label.world", fmt_lobby_world(0), is_client());
    draw_lobby_cycle(&ly, 1, "menu.lobby.label.enter_as",
        LFMT(get_peer_bool(get_local_peer(), "spectator") ? "value.spectator" : "value.player"), FALSE);
    draw_lobby_cycle(&ly, 2, "menu.lobby.label.character", get_character_name(CLIENT.character), character_disabled());
    draw_lobby_cycle(&ly, 3, "menu.lobby.label.powerup", fmt_lobby_powerup(0), character_disabled());

    draw_lobby_button(&ly, 4, LFMT("menu.lobby.button.options"), FALSE);
    if (is_host())
        draw_lobby_button(&ly, 5, LFMT("menu.lobby.button.kick"), kick_player_disabled());
    draw_lobby_button(&ly, 6,
        LFMT(get_lobby_player_count() >= 1
                 ? (is_client() ? "menu.lobby.button.waiting_for_host" : "menu.lobby.button.start")
                 : "menu.lobby.button.not_enough_players"),
        start_disabled());

    batch_pos(B_F3_XY(125.f, SCREEN_HEIGHT - 16.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    const char* ind = fmt("[%s] %s", kb_label(KB_PAUSE), LFMT("menu.disconnect"));
    batch_string_wrap("footer", 16.f, ind, 218.f);

    batch_pos(B_F3_XY(125.f, SCREEN_HEIGHT - 24.f - string_height_wrap("footer", 16.f, ind, 218.f)));
    batch_color(B_U4_ALPHA(200));
    batch_string_wrap("footer", 12.f, fmt("Checksum: %u", get_game_hash()), 218.f);

    // RIGHT
    Uint8 line = 0;
    for (const NetID* pids = get_peers(); *pids > 0; pids++) {
        const NetID pid = *pids;

        const float ly = 6.f + ((float)line * 59.f);

        batch_pos(B_F3_XY(265.f, ly));
        batch_color(B_U4_WHITE);
        batch_sprite("ui/menu/lobby/slot/peer");
        if (get_local_peer() == pid) {
            batch_blend(BM_ADD);
            batch_pos(B_F3_XY(266.f, ly + 1.f));
            batch_colors(B_U4X4({255, 255, 255, 255}, {128, 128, 128, 255}, {64, 64, 64, 255}, {32, 32, 32, 255}));
            batch_rectangle(NULL, B_F2(364.f, 52.f));
            batch_blend(BM_NORMAL);

            batch_color(B_U4_WHITE);
        }

        batch_pos(B_F3_XY(295.f, ly + 27.f));
        const Bool spectating = get_peer_bool(pid, "spectator");
        batch_color(B_U4_ALPHA(spectating ? 100 : 255));
        batch_sprite(fmt(get_character_cursor(get_peer_number(pid, "character")), 0));

        batch_pos(B_F3_XY(317.f, ly + 8.f));
        batch_color((get_master_peer() == pid) ? B_U4_YELLOW : B_U4_WHITE);
        batch_align(B_ALIGN_TOP_LEFT);
        const char* name = get_peer_name(pid);
        batch_string("footer", 16.f, name);

        batch_pos(B_F3_XY(322.f + string_width("footer", 16.f, name), ly + 8.f));
        batch_color(B_U4_ALPHA(200));
        if (spectating) {
            batch_pos(B_F3_XY(317.f, ly + 28.f));
            batch_string("footer", 16.f, LFMT("value.spectator"));
        } else {
            const PlayerPowerup powerup = get_peer_number(pid, "powerup");
            batch_string("footer", 16.f, fmt("x %i", DEFAULT_LIVES - get_powerup_cost(powerup)));

            batch_pos(B_F3_XY(317.f, ly + 28.f));
            batch_color((powerup == POW_NONE) ? B_U4_ALPHA(128) : B_U4_WHITE);
            batch_string("footer", 16.f, LFMT((powerup == POW_NONE) ? "value.no_powerup" : get_powerup_name(powerup)));
        }

        batch_pos(B_F3_XY(610.f, ly + 28.f));
        batch_color(B_U4_WHITE);
        batch_align(B_ALIGN(FA_RIGHT, FA_MIDDLE));
        batch_string("footer", 16.f, fmt("%i ms", get_peer_ping(pid)));

        ++line;
    }

    batch_color(B_U4_ALPHA(128));
    for (const Uint8 n = get_peer_limit(); line < n; line++) {
        batch_pos(B_F3_XY(265.f, 6.f + ((float)line * 59.f)));
        batch_sprite("ui/menu/lobby/slot/empty");
    }

    return FALSE;
}

// =======
// OPTIONS
// =======

static const char* fmt_world(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("option.world"), fmt_lobby_world(idx));
}

static void world_cycle(Sint8 cycle) {
    if (is_connected()) {
        const char* world = get_lobby_string("world");
        if (world != NULL)
            SDL_strlcpy(CLIENT.world, world, sizeof(CLIENT.world));
    }

    const char* wstr
        = (cycle > 0) ? next_world_from(CLIENT.world) : ((cycle < 0) ? last_world_from(CLIENT.world) : NULL);
    if (wstr == NULL)
        CLIENT.world[0] = '\0';
    else
        SDL_strlcpy(CLIENT.world, wstr, sizeof(CLIENT.world));
    update_lobby_data();
}

static const char* fmt_character(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("option.character"), get_character_name(CLIENT.character));
}

static Bool character_disabled() {
    return get_peer_bool(get_local_peer(), "spectator");
}

static void character_cycle(Sint8 cycle) {
    if (cycle > 0) {
        if (CLIENT.character >= (CHR_SIZE - 1))
            CLIENT.character = 0;
        else
            ++CLIENT.character;
    } else if (cycle < 0) {
        if (CLIENT.character <= 0)
            CLIENT.character = CHR_SIZE - 1;
        else
            --CLIENT.character;
    }

    update_peer_data();
}

static const char* fmt_powerup(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("option.powerup"), fmt_lobby_powerup(idx));
}

static void powerup_cycle(Sint8 cycle) {
    if (cycle > 0) {
        if (CLIENT.powerup >= (POW_SIZE - 1))
            CLIENT.powerup = 0;
        else
            ++CLIENT.powerup;
    } else if (cycle < 0) {
        if (CLIENT.powerup <= 0)
            CLIENT.powerup = POW_SIZE - 1;
        else
            --CLIENT.powerup;
    }

    update_peer_data();
}

static Bool start_disabled() {
    return is_client() || get_world(CLIENT.world) == NULL || get_lobby_player_count() < 1;
}

static void start_option() {
    if (is_client())
        return;

    const TinyHash key = StHashStr(CLIENT.world);
    if (get_world_key(key) == NULL)
        return;

    if (is_connected())
        peers_to_players();

    WorldContext ctx = init_world_context(key);
    jump_to_world(&ctx, TRUE);
}

static void saw_online_notice() {
    CLIENT.seen_online_notice = TRUE;
    save_config();
    set_menu(&CATALOG, MEN_MULTIPLAYER);
}

static void multiplayer_option() {
    if (CLIENT.seen_online_notice) {
        set_menu(&CATALOG, MEN_MULTIPLAYER);
        return;
    }

    UI* message = create_ui(UI_MESSAGE, NULL);
    if (message == NULL) {
        set_menu(&CATALOG, MEN_MULTIPLAYER);
        return;
    }

    UIMessageData* userdata = message->userdata;
    userdata->title = "message.notice";
    userdata->text = "message.online_notice";
    userdata->size = 24.f;
    userdata->verb = "menu.continue";
    userdata->cancel = saw_online_notice;
}

static const char* fmt_max_peers(size_t idx) {
    (void)idx;

    return fmt("%s: %u", LFMT("option.max_peers"), CLIENT.lobby_limit);
}

static void max_peers_cycle(Sint8 cycle) {
    if (cycle > 0) {
        if (CLIENT.lobby_limit >= MAX_PEERS)
            CLIENT.lobby_limit = 2;
        else
            ++CLIENT.lobby_limit;
    } else if (cycle < 0) {
        if (CLIENT.lobby_limit <= 2)
            CLIENT.lobby_limit = MAX_PEERS;
        else
            --CLIENT.lobby_limit;
    }
}

static const char* fmt_visibility(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("option.visibility"), LFMT(CLIENT.private_lobby ? "value.private" : "value.public"));
}

static void visibility_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.private_lobby = !CLIENT.private_lobby;
}

static const char* fmt_connection_failed() {
    const char* error = net_error();
    return (error == NULL) ? LFMT("message.connection_failed")
                           : fmt("%s\n(%s)", LFMT("message.connection_failed"), error);
}

static Bool wait_connecting() {
    switch (get_connect_state()) {
    default:
        return FALSE;

    case CONN_DISCONNECTED: {
        UI* message = create_ui(UI_MESSAGE, NULL);
        if (message == NULL)
            return TRUE;

        UIMessageData* userdata = message->userdata;
        userdata->fmt = fmt_connection_failed;

        return TRUE;
    }

    case CONN_CONNECTED:
        return TRUE;
    }
}

static void finish_connecting() {
    if (is_connected()) {
        set_menu(&CATALOG, MEN_LOBBY);
        play_generic_sound("ui/connect", PLAY_SYSTEM);
    }
}

static void prompt_connect() {
    UI* message = create_ui(UI_MESSAGE, NULL);
    if (message == NULL)
        return;

    UIMessageData* userdata = message->userdata;
    userdata->text = "message.connecting";
    userdata->verb = "menu.cancel";
    userdata->wait = wait_connecting;
    userdata->finish = finish_connecting;
    userdata->cancel = disconnect;
}

static void host_option() {
    host_lobby();
    prompt_connect();
}

static void lobby_option() {
    const LobbyInfo* lobby = get_lobby_list(CATALOG.menus[MEN_LOBBY_LIST].option);
    if (lobby == NULL)
        return;

    join_lobby(lobby->id);
    prompt_connect();
}

static void enter_as_cycle(Sint8 cycle) {
    (void)cycle;

    toggle_spectator();
}

static Bool kick_player_disabled() {
    return is_client() || get_peer_count() <= 1;
}

static void kick_player_option() {
    create_ui(UI_KICK, NULL);
}

static const char* fmt_replay_error() {
    return fmt("%s\n%s", LFMT("message.replay_load_error"), LFMT(replay_error));
}

static void replay_option() {
    replay_error
        = load_replay(fmt("replays/%s.rpl", CATALOG.options[MEN_REPLAYS][CATALOG.menus[MEN_REPLAYS].option].name));
    if (replay_error == NULL)
        return;

    UI* message = create_ui(UI_MESSAGE, NULL);
    if (message != NULL)
        ((UIMessageData*)message->userdata)->fmt = fmt_replay_error;
}

static void options_option() {
    create_ui(UI_OPTIONS, NULL);
}

static void exit_option() {
    set_screen(SCR_EXIT, NULL, 0);
}

static const char* fmt_test_level(size_t idx) {
    (void)idx;

    const Level* level = get_level(CLIENT.level);
    return fmt("%s: %s", LFMT("option.test_level"), (level == NULL) ? NULL : LFMT(fmt("level.%s", level->name)));
}

static void test_level_cycle(Sint8 cycle) {
    const char* lstr
        = (cycle > 0) ? next_level_from(CLIENT.level) : ((cycle < 0) ? last_level_from(CLIENT.level) : NULL);
    if (lstr == NULL)
        CLIENT.level[0] = '\0';
    else
        SDL_strlcpy(CLIENT.level, lstr, sizeof(CLIENT.level));
}

static void test_level_option() {
    WorldContext wctx = init_world_context(0);
    start_world(&wctx);

    GameContext gctx = init_game_context(worldcontext(), StHashStr(CLIENT.level));
    jump_to_game(&gctx, TRUE);
}

static void go_to_editor_option() {
    set_screen(SCR_EDITOR, NULL, 0);
}

// ======
// SCREEN
// ======

static void start(const void* secret, size_t secret_size) {
    load_sprite("ui/backgrounds/main", AKL_NEVER);
    load_sprite("ui/backgrounds/options", AKL_NEVER);
    load_sprite("ui/backgrounds/lobby", AKL_NEVER);
    load_sprite("logos/mario_together", AKL_NEVER);
    load_sprite("ui/menu/buttons/singleplayer", AKL_NEVER);
    load_sprite("ui/menu/buttons/multiplayer", AKL_NEVER);
    load_sprite("ui/menu/buttons/options", AKL_NEVER);
    load_sprite("ui/menu/buttons/replays", AKL_NEVER);
    load_sprite("ui/menu/buttons/editor", AKL_NEVER);
    load_sprite("ui/menu/buttons/exit", AKL_NEVER);
    load_sprite("ui/menu/icons/game", AKL_NEVER);
    load_sprite("ui/menu/icons/editor", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.world", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.enter_as", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.character", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.powerup", AKL_NEVER);
    load_sprite("ui/menu/lobby/arrow", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.options", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.kick", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.start", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.not_enough_players", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.waiting_for_host", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/empty", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/peer", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/you", AKL_NEVER);
    load_font("menu", AKL_NEVER);
    load_sound("ui/enter", AKL_ONCE);
    load_sound("ui/connect", AKL_NEVER);
    load_sound("ui/disconnect", AKL_NEVER);
    load_track("title", AKL_NEVER);

    // Handle invite JSON
    Bool got_invite = FALSE;

    if (secret == NULL)
        goto no_secret;

    yyjson_doc* json = read_json(secret, secret_size, NULL);
    if (json == NULL)
        goto no_secret;

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(json);
        goto no_secret;
    }

    const char* server = yyjson_get_str(yyjson_obj_get(root, "server"));
    const NetID lid = yyjson_get_uint(yyjson_obj_get(root, "lobby"));
    if (server == NULL || lid <= 0) {
        yyjson_doc_free(json);
        goto no_secret;
    }

    set_hostname(server);
    join_lobby(lid);
    prompt_connect();
    got_invite = TRUE;

    yyjson_doc_free(json);

no_secret:
    (void)0;

    const MenuType last_menu = CATALOG.current;
    if ((got_invite || !is_connected()) && CATALOG.current == MEN_LOBBY)
        previous_menu(&CATALOG);

    if (last_menu == CATALOG.current && CATALOG.current > MEN_NULL && CATALOG.current < MEN_SIZE) {
        Menu* menu = &CATALOG.menus[CATALOG.current];
        if (menu->enter != NULL)
            menu->enter(menu->from);
    }

    play_generic_track("title", PLAY_LOOPING, 0);
    fade_generic_track(1.f, 100.f);
}

static void end() {
    leave_replays_menu(MEN_NULL); // GROSS HACK: Make sure to free allocated strings when leaving this screen.
}

static void tick() {
    tick_catalog(&CATALOG, NULL);
}

static void draw() {
    clear_color(B_F4_VALUE(0.f));
}

static void draw_ui() {
    batch_reset();

    const UI* ui = topui();
    if (CATALOG.current == MEN_MAIN && ui == NULL) {
        batch_sprite("ui/backgrounds/main");
        batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 60.f + SDL_roundf(SDL_sinf(screenticks() * 0.03f) * 7.f)));
        batch_sprite("logos/mario_together");
    } else {
        batch_sprite((CATALOG.current == MEN_LOBBY && ui == NULL) ? "ui/backgrounds/lobby" : "ui/backgrounds/options");
    }

    if (ui != NULL)
        return;

    draw_catalog(&CATALOG);

    if (CATALOG.current == MEN_MAIN) {
        const float t = screenticks();
        if (t < 45.f) {
            batch_reset();
            batch_pos(B_F3_XY(-1000.f, -1000.f));
            batch_color(B_U4_ALPHA((1.f - (t / 45.f)) * 255.f));
            batch_rectangle(NULL, B_F2_S(3000.f));
        }
    }
}

static Transition transit() {
    Transition transition = {0};
    transition.type = TRANS_CIRCLE;
    transition.duration = 50.5f;

    fade_generic_track(0.f, 25.f);

    return transition;
}

const ScreenTable TAB_MENU = {
    .start = start,
    .end = end,
    .tick = tick,
    .draw = draw,
    .draw_ui = draw_ui,
    .transit = transit,
};
