#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_file.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
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

    MEN_SIZE,
};

static const char* replay_error = NULL;

static void enter_replays_menu(MenuType), leave_replays_menu(MenuType), enter_lobby_list_menu(MenuType),
    tick_lobby_list_menu(), enter_lobby_menu(MenuType), leave_lobby_menu(MenuType), tick_lobby_menu();
static Bool draw_main_menu(), draw_replays_menu(), draw_lobby_menu(), kick_player_disabled(), start_disabled();

static const char *fmt_max_peers(size_t), *fmt_visibility(size_t), *fmt_lobby(), *fmt_character(size_t),
    *fmt_powerup(size_t), *fmt_enter_as(size_t), *fmt_world(size_t), *fmt_start(size_t), *fmt_kick_player(size_t);
static void multiplayer_option(), options_option(), max_peers_cycle(Sint8), visibility_cycle(Sint8), host_option(),
    character_cycle(Sint8), powerup_cycle(Sint8), enter_as_cycle(Sint8), kick_player_option(), world_cycle(Sint8);

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

        [MEN_LOBBY] = {
			.fmt = fmt_lobby,
			.leave = leave_lobby_menu,
			.tick = tick_lobby_menu,
			.draw = draw_lobby_menu
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

        [MEN_SINGLEPLAYER] = {
            {.fmt = fmt_world, .cycle = world_cycle},
            {},
            {.fmt = fmt_character, .cycle = character_cycle},
            {.fmt = fmt_powerup, .cycle = powerup_cycle},
            {},
            {.fmt = fmt_start, .disabled = start_disabled},
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

        [MEN_LOBBY] = {
            {.fmt = fmt_world, .disabled = is_client, .cycle = world_cycle},
            {.fmt = fmt_enter_as, .cycle = enter_as_cycle},
            {.fmt = fmt_character, .cycle = character_cycle},
            {.fmt = fmt_powerup, .cycle = powerup_cycle},
            {.name = "opt_options", .callback = options_option},
            {.fmt = fmt_kick_player, .disabled = kick_player_disabled, .callback = kick_player_option},
            {.fmt = fmt_start, .disabled = start_disabled},
        }
	}
};

// =====
// MENUS
// =====

static Bool draw_main_menu() {
    draw_options(CATALOG.options[MEN_MAIN], CATALOG.menus[MEN_MAIN].option, HALF_SCREEN_HEIGHT);

    batch_reset();
    batch_pos(B_XY(64.f, SCREEN_HEIGHT - 24.f));
    batch_color(B_ALPHA(160));
    batch_align(B_BOTTOM_LEFT);
    batch_string("main", 24.f, GAME_NAME " " GAME_VERSION);
    batch_pos(B_XY(SCREEN_WIDTH - 64.f, SCREEN_HEIGHT - 24.f));
    batch_align(B_BOTTOM_RIGHT);
    batch_string("main", 24.f, fmt("GameHash: %u", get_game_hash()));

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

static void enter_lobby_list_menu(MenuType) {
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
    CATALOG.options[MEN_LOBBY_LIST][0].name = "opt_no_lobbies";

    for (Uint8 i = 0; i < MAX_OPTIONS; i++) {
        const LobbyInfo* lobby = get_lobby_list(i);
        if (lobby == NULL)
            continue;

        CATALOG.options[MEN_LOBBY_LIST][i].fmt = fmt_lobby_list;
        CATALOG.options[MEN_LOBBY_LIST][i].callback = lobby_option;
    }
}

static void replay_option();
static void iterate_replay_file(const char* filename, const void*, size_t, void* userdata) {
    size_t* idx = userdata;
    if (*idx >= MAX_OPTIONS)
        return;

    Option* option = &CATALOG.options[MEN_REPLAYS][*idx];
    option->name = SDL_strdup(filename_no_ext(file_basename(filename)));
    option->callback = replay_option;

    ++*idx;
}

static void enter_replays_menu(MenuType) {
    leave_replays_menu(MEN_NULL);
    iterate_user_files("replays/*.rpl", FALSE, iterate_replay_file, &(size_t){0});
}

static void leave_replays_menu(MenuType) {
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

static const char* fmt_lobby() {
    const char* lname = get_lobby_name();
    return fmt("%s (%s)", (lname != NULL && SDL_strnlen(lname, 33) > 32) ? fmt("%.*s...", 32, lname) : lname,
        LFMT(in_private_lobby() ? "val_private" : "val_public"));
}

static void leave_lobby_menu(MenuType) {
    disconnect();
    play_generic_sound("ui/disconnect", PLAY_SYSTEM);
}

static const char* fmt_disconnected() {
    const char* error = net_error();
    return (error == NULL) ? LFMT("msg_disconnected") : fmt("%s\n(%s)", LFMT("msg_disconnected"), error);
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
        userdata->title = "msg_error";
        userdata->fmt = fmt_disconnected;
        userdata->cancel = cancel_error;
    }
}

static Bool draw_lobby_menu() {
    batch_reset();

    float py = HALF_SCREEN_HEIGHT + 40.f;
    static const float PEER_SIZE = 16.f;

    batch_pos(B_XY(16.f, py));
    batch_string("main", PEER_SIZE, LFMT("opt_peer"));
    batch_pos(B_XY(168.f, py));
    batch_string("main", PEER_SIZE, LFMT("opt_character"));
    batch_pos(B_XY(320.f, py));
    batch_string("main", PEER_SIZE, LFMT("opt_powerup"));
    batch_pos(B_XY(472.f, py));
    batch_string("main", PEER_SIZE, LFMT("opt_ping"));

    py += PEER_SIZE;

    Uint8 line = 0;
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++) {
        const NetID pid = *ptr;
        const Bool is_master = get_master_peer() == pid;
        batch_color((get_local_peer() == pid) ? (is_master ? B_RGB(255, 144, 80) : B_YELLOW)
                                              : (is_master ? B_RGB(255, 160, 160) : B_WHITE));

        batch_pos(B_XY(16.f, py));
        batch_string("main", PEER_SIZE, get_peer_name(pid));

        batch_pos(B_XY(168.f, py));
        batch_string("main", PEER_SIZE,
            (get_peer_number(pid, "spectator") > 0) ? LFMT("val_spectator")
                                                    : get_character_name(get_peer_number(pid, "character")));

        batch_pos(B_XY(320.f, py));
        batch_string("main", PEER_SIZE, get_powerup_name(get_peer_number(pid, "powerup")));

        batch_pos(B_XY(472.f, py));
        batch_string("main", PEER_SIZE, fmt("%i ms", get_peer_ping(pid)));

        py += PEER_SIZE;
        ++line;
    }

    batch_color(B_WHITE);
    for (const Uint8 n = get_peer_limit(); line < n; line++) {
        batch_pos(B_XY(16.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_XY(168.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_XY(320.f, py));
        batch_string("main", PEER_SIZE, "-");
        batch_pos(B_XY(472.f, py));
        batch_string("main", PEER_SIZE, "-");

        py += PEER_SIZE;
    }

    return TRUE;
}

// =======
// OPTIONS
// =======

static const char* fmt_world(size_t) {
    const World* world = get_world(is_connected() ? get_lobby_string("world") : CLIENT.world);
    return fmt("%s: %s", LFMT("opt_world"), (world == NULL) ? NULL : LFMT(fmt("wld_%s", world->name)));
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

static const char* fmt_character(size_t) {
    return fmt("%s: %s", LFMT("opt_character"), get_character_name(CLIENT.character));
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

static const char* fmt_powerup(size_t) {
    const Sint8 cost = get_powerup_cost(CLIENT.powerup);
    return fmt(
        "%s: %s%s", LFMT("opt_powerup"), get_powerup_name(CLIENT.powerup), (cost > 0) ? fmt(" (-%i)", cost) : "");
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

static const char* fmt_start(size_t) {
    if (is_client())
        return LFMT("opt_waiting_for_host");

    if (get_world(CLIENT.world) == NULL)
        return LFMT("opt_invalid_world");

    return LFMT("opt_start");
}

static Bool start_disabled() {
    return is_client() || get_world(CLIENT.world) == NULL;
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
    userdata->title = "msg_notice";
    userdata->text = "msg_online_notice";
    userdata->font = "main";
    userdata->size = 24.f;
    userdata->verb = "continue";
    userdata->cancel = saw_online_notice;
}

static const char* fmt_max_peers(size_t) {
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

static const char* fmt_visibility(size_t) {
    return fmt("%s: %s", LFMT("opt_visibility"), LFMT(CLIENT.private_lobby ? "val_private" : "val_public"));
}

static void visibility_cycle(Sint8) {
    CLIENT.private_lobby = !CLIENT.private_lobby;
}

static Bool wait_connecting() {
    switch (get_connect_state()) {
    default:
        return FALSE;

    case CONN_FAIL: {
        UI* message = create_ui(UI_MESSAGE, NULL);
        if (message == NULL)
            return TRUE;

        UIMessageData* userdata = message->userdata;
        userdata->text = "msg_connection_failed";
    }

    case CONN_SUCCESS:
        return TRUE;
    }
}

static void finish_connecting() {
    if (get_connect_state() == CONN_SUCCESS) {
        set_menu(&CATALOG, MEN_LOBBY);
        play_generic_sound("ui/connect", PLAY_SYSTEM);
    }
}

static void prompt_connect() {
    UI* message = create_ui(UI_MESSAGE, NULL);
    if (message == NULL)
        return;

    UIMessageData* userdata = message->userdata;
    userdata->text = "msg_connecting";
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

static const char* fmt_enter_as(size_t) {
    return fmt("%s: %s", LFMT("opt_enter_as"),
        LFMT((get_peer_number(get_local_peer(), "spectator") > 0) ? "val_spectator" : "val_player"));
}

static void enter_as_cycle(Sint8) {
    toggle_spectator();
}

static const char* fmt_kick_player(size_t) {
    return is_client() ? NULL : LFMT("opt_kick_player");
}

static Bool kick_player_disabled() {
    return is_client() || get_peer_count() <= 1;
}

static void kick_player_option() {
    create_ui(UI_KICK, NULL);
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

static void start(const void* secret, size_t secret_size) {
    load_sprite("ui/backgrounds/main", FALSE);
    load_sprite("ui/backgrounds/options", FALSE);
    load_sprite("logos/mario_forever", FALSE);
    load_sound("ui/connect", FALSE);
    load_sound("ui/disconnect", FALSE);
    load_track("title", FALSE);

    play_generic_track(GTS_MAIN, "title", PLAY_LOOPING);

    // Handle invite JSON
    Bool got_invite = FALSE;

    if (secret == NULL)
        goto s_no_secret;

    yyjson_doc* json = read_json(secret, secret_size);
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

s_no_secret:
    if (got_invite || !is_connected())
        for (MenuType i = 0; i < (MenuType)MEN_SIZE; i++)
            CATALOG.menus[i].from = MEN_NULL;
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
    if (CATALOG.current == MEN_MAIN && ui == NULL) {
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
