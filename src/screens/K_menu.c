#include <SDL3/SDL_platform_defines.h>

#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_file.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"
#include "K_worlds.h"

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

static const char* credits[6][2] = {
    {"menu.credits.mario_forever",  "menu.credits.mario_forever.text" },
    {"menu.credits.graphics",       "menu.credits.graphics.text"      },
    {"menu.credits.programming",    "menu.credits.programming.text"   },
    {"menu.credits.beta_testing",   "menu.credits.beta_testing.text"  },
    {"menu.credits.special_thanks", "menu.credits.special_thanks.text"},
    {NULL,                          "menu.credits.end"                },
};

static const char* replay_error = NULL;

static void enter_replays_menu(MenuType), leave_replays_menu(MenuType), enter_lobby_list_menu(MenuType),
    tick_lobby_list_menu(), enter_lobby_menu(MenuType), leave_lobby_menu(MenuType), tick_lobby_menu();
static Bool draw_main_menu(), draw_replays_menu(), draw_lobby_menu(), kick_player_disabled(), start_disabled(),
    character_disabled();

static const char *fmt_max_peers(size_t), *fmt_visibility(size_t), *fmt_lobby(), *fmt_character(size_t),
    *fmt_powerup(size_t), *fmt_enter_as(size_t), *fmt_world(size_t), *fmt_start(size_t), *fmt_kick_player(size_t),
    *fmt_test_level(size_t);
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
            {.fmt = fmt_start, .disabled = start_disabled, .callback = start_option},
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
            {.fmt = fmt_world, .disabled = is_client, .cycle = world_cycle},
            {.fmt = fmt_enter_as, .cycle = enter_as_cycle},
            {.fmt = fmt_character, .disabled = character_disabled, .cycle = character_cycle},
            {.fmt = fmt_powerup, .disabled = character_disabled, .cycle = powerup_cycle},
            {.name = "option.options", .callback = options_option},
            {.fmt = fmt_kick_player, .disabled = kick_player_disabled, .callback = kick_player_option},
            {.fmt = fmt_start, .disabled = start_disabled, .callback = start_option},
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
    CATALOG.options[MEN_LOBBY_LIST][0].name = "option.no_lobbies";

    for (Uint8 i = 0; i < MAX_OPTIONS; i++) {
        const LobbyInfo* lobby = get_lobby_list(i);
        if (lobby == NULL)
            continue;

        CATALOG.options[MEN_LOBBY_LIST][i].fmt = fmt_lobby_list;
        CATALOG.options[MEN_LOBBY_LIST][i].callback = lobby_option;
    }
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

static Bool draw_lobby_menu() {
    batch_reset();

    float py = HALF_SCREEN_HEIGHT + 40.f;
    static const float PEER_SIZE = 16.f;

    batch_pos(B_F3_XY(16.f, py));
    batch_string("main", PEER_SIZE, LFMT("option.peer"));
    batch_pos(B_F3_XY(168.f, py));
    batch_string("main", PEER_SIZE, LFMT("option.character"));
    batch_pos(B_F3_XY(320.f, py));
    batch_string("main", PEER_SIZE, LFMT("option.powerup"));
    batch_pos(B_F3_XY(472.f, py));
    batch_string("main", PEER_SIZE, LFMT("option.ping"));

    py += PEER_SIZE;

    Uint8 line = 0;
    for (const NetID* pids = get_peers(); *pids > 0; pids++) {
        const NetID pid = *pids;
        const Bool is_master = get_master_peer() == pid;
        batch_color((get_local_peer() == pid) ? (is_master ? B_U4_RGB(255, 144, 80) : B_U4_YELLOW)
                                              : (is_master ? B_U4_RGB(255, 160, 160) : B_U4_WHITE));

        batch_pos(B_F3_XY(16.f, py));
        batch_string("main", PEER_SIZE, get_peer_name(pid));

        batch_pos(B_F3_XY(168.f, py));
        batch_string("main", PEER_SIZE,
            (get_peer_bool(pid, "spectator")) ? LFMT("value.spectator")
                                              : get_character_name(get_peer_number(pid, "character")));

        batch_pos(B_F3_XY(320.f, py));
        batch_string("main", PEER_SIZE, get_powerup_name(get_peer_number(pid, "powerup")));

        batch_pos(B_F3_XY(472.f, py));
        batch_string("main", PEER_SIZE, fmt("%i ms", get_peer_ping(pid)));

        py += PEER_SIZE;
        ++line;
    }

    batch_color(B_U4_WHITE);
    for (const Uint8 n = get_peer_limit(); line < n; line++) {
        batch_pos(B_F3_XY(16.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_F3_XY(168.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_F3_XY(320.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_F3_XY(472.f, py));
        batch_string("main", PEER_SIZE, "-");

        py += PEER_SIZE;
    }

    return TRUE;
}

// =======
// OPTIONS
// =======

static const char* fmt_world(size_t idx) {
    (void)idx;

    const World* world = get_world(is_connected() ? get_lobby_string("world") : CLIENT.world);
    return fmt("%s: %s", LFMT("option.world"), (world == NULL) ? NULL : LFMT(fmt("world.%s", world->name)));
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

    const Sint8 cost = get_powerup_cost(CLIENT.powerup);
    return fmt(
        "%s: %s%s", LFMT("option.powerup"), get_powerup_name(CLIENT.powerup), (cost > 0) ? fmt(" (-%i)", cost) : "");
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

static const char* fmt_start(size_t idx) {
    (void)idx;

    if (get_world(CLIENT.world) == NULL)
        return LFMT("option.invalid_world");

    if (get_lobby_player_count() < 1)
        return LFMT("option.not_enough_players");

    if (is_client())
        return LFMT("option.waiting_for_host");

    return LFMT("option.start");
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
    userdata->verb = "continue";
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

static Bool wait_connecting() {
    switch (get_connect_state()) {
    default:
        return FALSE;

    case CONN_DISCONNECTED: {
        UI* message = create_ui(UI_MESSAGE, NULL);
        if (message == NULL)
            return TRUE;

        UIMessageData* userdata = message->userdata;
        userdata->text = "message.connection_failed";

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
    userdata->verb = "cancel";
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

static const char* fmt_enter_as(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("option.enter_as"),
        LFMT((get_peer_bool(get_local_peer(), "spectator")) ? "value.spectator" : "value.player"));
}

static void enter_as_cycle(Sint8 cycle) {
    (void)cycle;

    toggle_spectator();
}

static const char* fmt_kick_player(size_t idx) {
    (void)idx;

    return is_client() ? NULL : LFMT("option.kick_player");
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
    load_sprite("logos/mario_together", AKL_NEVER);
    load_sprite("ui/menu/buttons/singleplayer", AKL_NEVER);
    load_sprite("ui/menu/buttons/multiplayer", AKL_NEVER);
    load_sprite("ui/menu/buttons/options", AKL_NEVER);
    load_sprite("ui/menu/buttons/replays", AKL_NEVER);
    load_sprite("ui/menu/buttons/editor", AKL_NEVER);
    load_sprite("ui/menu/buttons/exit", AKL_NEVER);
    load_sprite("ui/menu/icons/game", AKL_NEVER);
    load_sprite("ui/menu/icons/editor", AKL_NEVER);
    load_font("menu", AKL_NEVER);
    load_font("footer", AKL_NEVER);
    load_sound("ui/enter", AKL_ONCE);
    load_sound("ui/connect", AKL_NEVER);
    load_sound("ui/disconnect", AKL_NEVER);
    load_track("doxeh_remix", AKL_NEVER);

    // Handle invite JSON
    Bool got_invite = FALSE;

    if (secret == NULL)
        goto s_no_secret;

    yyjson_doc* json = read_json(secret, secret_size, NULL);
    if (json == NULL)
        goto s_no_secret;

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(json);
        goto s_no_secret;
    }

    const char* server = yyjson_get_str(yyjson_obj_get(root, "server"));
    const NetID lid = yyjson_get_uint(yyjson_obj_get(root, "lobby"));
    if (server == NULL || lid <= 0) {
        yyjson_doc_free(json);
        goto s_no_secret;
    }

    set_hostname(server);
    join_lobby(lid);
    prompt_connect();
    got_invite = TRUE;

    yyjson_doc_free(json);

s_no_secret:
    (void)0;

    const MenuType last_menu = CATALOG.current;
    if ((got_invite || !is_connected()) && CATALOG.current == MEN_LOBBY)
        previous_menu(&CATALOG);

    if (last_menu == CATALOG.current && CATALOG.current > MEN_NULL && CATALOG.current < MEN_SIZE) {
        Menu* menu = &CATALOG.menus[CATALOG.current];
        if (menu->enter != NULL)
            menu->enter(menu->from);
    }

    play_generic_track("doxeh_remix", PLAY_LOOPING, 0);
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
        batch_sprite("ui/backgrounds/options");
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
