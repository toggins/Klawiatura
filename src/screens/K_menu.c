#include "K_audio.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

enum {
    MEN_NULL,

    MEN_MAIN,
    MEN_SINGLEPLAYER,
    MEN_MULTIPLAYER,
    MEN_REPLAYS,
    MEN_HOST_LOBBY,
    MEN_LOBBY_LIST,
    MEN_LOBBY,

    MEN_SIZE,
};

static const char* replay_error = NULL;

static void enter_replays_menu(MenuType), leave_replays_menu(MenuType), enter_lobby_list_menu(MenuType),
    tick_lobby_list_menu();
static Bool draw_main_menu(), draw_replays_menu();

static const char *fmt_max_peers(size_t), *fmt_visibility(size_t);
static void multiplayer_option(), options_option(), max_peers_cycle(Sint8), visibility_cycle(Sint8), host_option();

static Catalog CATALOG = {
	.current = MEN_MAIN,

	.menus = {
        [MEN_MAIN] = {
            .name = "title",
            .draw = draw_main_menu,
        },

		[MEN_SINGLEPLAYER] = {
			.name = "opt_singleplayer",
		},

		[MEN_MULTIPLAYER] = {
			.name = "opt_multiplayer",
		},

        [MEN_REPLAYS] = {
            .name = "opt_replays",
            .enter = enter_replays_menu,
            .leave = leave_replays_menu,
            .draw = draw_replays_menu,
        },

		[MEN_HOST_LOBBY] = {
			.name = "opt_host_lobby",
		},

		[MEN_LOBBY_LIST] = {
			.name = "opt_find_lobby",
			.enter = enter_lobby_list_menu,
			.tick = tick_lobby_list_menu
		},
	},

	.options = {
		[MEN_MAIN] = {
			{.name = "opt_singleplayer", .menu = MEN_SINGLEPLAYER},
			{.name = "opt_multiplayer", .callback = multiplayer_option},
            {.name = "opt_replays", .menu = MEN_REPLAYS},
			{.name = "opt_options", .callback = options_option},
#ifndef SDL_PLATFORM_EMSCRIPTEN
			{.name = "opt_exit", .callback = permadeath},
#endif
		},

		[MEN_MULTIPLAYER] = {
			{.name = "opt_host_lobby", .menu = MEN_HOST_LOBBY},
			{.name = "opt_find_lobby", .menu = MEN_LOBBY_LIST},
		},

		[MEN_HOST_LOBBY] = {
			{.fmt = fmt_max_peers, .cycle = max_peers_cycle},
			{.fmt = fmt_visibility, .cycle = visibility_cycle},
			{},
			{.name = "opt_host", .callback = host_option},
		},
	}
};

// =====
// MENUS
// =====

static Bool draw_main_menu() {
    draw_options(CATALOG.options[MEN_MAIN], CATALOG.menus[MEN_MAIN].option, HALF_SCREEN_HEIGHT);

    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 24.f));
    batch_color(B_ALPHA(160));
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("main", 24.f, GAME_NAME " " GAME_VERSION);

    const char* credits = LFMT("credits");
    const float wrap = string_width("main", 16.f, credits) + SCREEN_WIDTH;
    const float scroll = SDL_fmodf(totalticks(), wrap);

    batch_pos(B_XY(SCREEN_WIDTH - scroll, SCREEN_HEIGHT - 24.f));
    batch_color(B_ALPHA(((scroll < 64.f) ? (scroll / 64.f)
                                         : ((scroll > (wrap - 64.f)) ? (1.f - ((scroll - (wrap - 64.f)) / 64.f)) : 1.f))
                        * 160.f));
    batch_align(B_TOP_LEFT);
    batch_string("main", 16.f, credits);

    return FALSE;
}

static void enter_lobby_list_menu(MenuType from) {
    (void)from;

    find_lobbies();
    SDL_zeroa(CATALOG.options[MEN_LOBBY_LIST]);
}

static const char* fmt_lobby_list(size_t idx) {
    const LobbyInfo* lobby = get_lobby_list(idx);
    if (lobby == NULL)
        return NULL;

    return fmt("%s (%u/%u)", lobby->name, lobby->peers, lobby->capacity);
}

static void lobby_option();
static void tick_lobby_list_menu() {
    SDL_zeroa(CATALOG.options[MEN_LOBBY_LIST]);
    CATALOG.options[MEN_LOBBY_LIST][0].name = "opt_no_lobbies_yet";

    for (Uint8 i = 0; i < MAX_OPTIONS; i++) {
        const LobbyInfo* lobby = get_lobby_list(i);
        if (lobby == NULL)
            continue;

        CATALOG.options[MEN_LOBBY_LIST][i].fmt = fmt_lobby_list;
        CATALOG.options[MEN_LOBBY_LIST][i].callback = lobby_option;
    }
}

static void replay_option();
static void iterate_replay_file(const char* filename, void* buffer, size_t size, void* userdata) {
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
    batch_pos(B_HALF_SCREEN);
    batch_colors(B_MF_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("header", 32.f, LFMT("opt_no_replays"));
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("opt_how_to_record", 's', kb_label(KB_RECORD_REPLAY)));

    return TRUE;
}

// =======
// OPTIONS
// =======

static void multiplayer_option() {
    // TODO
}

static const char* fmt_max_peers(size_t idx) {
    (void)idx;

    return fmt("%s: %u", LFMT("opt_max_peers"), CLIENT.lobby_limit);
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

    return fmt("%s: %s", LFMT("opt_visibility"), LFMT(CLIENT.private_lobby ? "val_private" : "val_public"));
}

static void visibility_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.private_lobby = !CLIENT.private_lobby;
}

static void host_option() {
    // TODO
}

static void lobby_option() {
    // TODO
}

static void replay_option() {
    // TODO
}

static void options_option() {
    create_ui(UI_OPTIONS, NULL);
}

// ======
// SCREEN
// ======

static void start(const char* secret) {
    (void)secret;

    load_sprite("ui/backgrounds/main", FALSE);
    load_sprite("ui/backgrounds/options", FALSE);
    load_sprite("logos/mario_forever", FALSE);
    load_track("title", FALSE);

    play_generic_track(GTS_MAIN, "title", PLAY_LOOPING);
}

static void end() {
    leave_replays_menu(MEN_NULL); // GROSS HACK: Make sure to free allocated strings when leaving this screen.
}

static void tick() {
    tick_catalog(&CATALOG, NULL);
}

static void draw_ui() {
    batch_reset();

    const UI* ui = topui();
    if (CATALOG.current == MEN_MAIN && (ui == NULL || ui->type != UI_OPTIONS)) {
        batch_sprite("ui/backgrounds/main");
        batch_pos(B_XY(HALF_SCREEN_WIDTH, 116.f));
        batch_sprite("logos/mario_forever");
    } else {
        batch_sprite("ui/backgrounds/options");
    }

    if (ui != NULL)
        return;

    draw_catalog(&CATALOG);
}

const ScreenTable TAB_MENU = {
    .start = start,
    .end = end,
    .tick = tick,
    .draw_ui = draw_ui,
};
