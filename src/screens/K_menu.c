#include "K_audio.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#define CREDITS_SIZE 18

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

static const char* credits[] = {
    "This is a free, open-source",
    "project not created for any sort",
    "of profit.",
    NULL,
    "We do not condone any commercial",
    "use of this project.",
    NULL,
    "Mario and related characters",
    "(c) Nintendo.",
    NULL,
    "[Mario Forever]",
    "Michal Gdaniec",
    "Maurycy Zarzycki",
    "Lukham",
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
    NULL,
    "If you're itching for more, play",
    "Mario Forever: Community Edition",
    "at mfce.rnx.su!",
};
static float credits_y = SCREEN_HEIGHT;

static const char* replay_error = NULL;

static void enter_replays_menu(MenuType), leave_replays_menu(MenuType), enter_lobby_list_menu(MenuType),
    tick_lobby_list_menu();
static Bool draw_main_menu();

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
    credits_y -= deltaticks() * 0.625f;

    const size_t n = SDL_arraysize(credits);
    if (credits_y <= ((float)n * -CREDITS_SIZE))
        credits_y = SCREEN_HEIGHT;

    batch_reset();
    for (size_t i = 0; i < n; i++) {
        const float y = credits_y + ((float)i * CREDITS_SIZE);
        if (y >= (SCREEN_HEIGHT - CREDITS_SIZE) || y <= 0.f)
            continue;

        const float top = 200.f, bottom = SCREEN_HEIGHT - 200.f;
        if ((y + CREDITS_SIZE) > bottom)
            batch_color(B_ALPHA(128 - (Uint8)(((y + CREDITS_SIZE - bottom) / 200.f) * 128.f)));
        else if (y < top)
            batch_color(B_ALPHA(128 - (Uint8)(((top - y) / 200.f) * 128.f)));
        else
            batch_color(B_ALPHA(128));

        batch_pos(B_XY(SCREEN_WIDTH - 32.f, y));
        batch_align(B_TOP_RIGHT);
        batch_string("main", CREDITS_SIZE, credits[i]);
    }

    return TRUE;
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
    // TODO
}

// ======
// SCREEN
// ======

static void start(const char* secret) {
    (void)secret;

    load_sprite("ui/background", FALSE);
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
    batch_sprite("ui/background");

    if (topui())
        return;

    draw_catalog(&CATALOG);
}

const ScreenTable TAB_MENU = {
    .start = start,
    .end = end,
    .tick = tick,
    .draw_ui = draw_ui,
};
