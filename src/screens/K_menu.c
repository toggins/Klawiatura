#include <SDL3/SDL_platform_defines.h>

#include "K_audio.h"
#include "K_cmake.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_input.h"
#include "K_levels.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "uis/K_kick.h"
#include "uis/K_message.h"

enum {
    MEN_NULL,

    MEN_MAIN,
    MEN_SINGLEPLAYER,
    MEN_SINGLEPLAYER_CHARACTER,
    MEN_MULTIPLAYER,
    MEN_REPLAYS,
    MEN_HOST_LOBBY,
    MEN_LOBBY_LIST,
    MEN_LOBBY,
    MEN_EDITOR,

    MEN_SIZE,
};

typedef Uint8 ReplayInfoStatus;
enum {
    RIS_OK,
    RIS_BAD_HEADER,
    RIS_BAD_VERSION,
    RIS_BAD_CHECKSUM,
};

typedef struct {
    ReplayInfoStatus status;
    PlayerID num_players;
    Uint32 checksum;
    TinyHash level;

    const char *filename, *name, *version;
} ReplayInfo;

static const char* credits[8][2] = {
    {"menu.credits.mario_forever",  "menu.credits.mario_forever.text" },
    {"menu.credits.graphics",       "menu.credits.graphics.text"      },
    {"menu.credits.audio",          "menu.credits.audio.text"         },
    {"menu.credits.programming",    "menu.credits.programming.text"   },
    {"menu.credits.levels",         "menu.credits.levels.text"        },
    {"menu.credits.beta_testing",   "menu.credits.beta_testing.text"  },
    {"menu.credits.special_thanks", "menu.credits.special_thanks.text"},
    {"menu.credits.end",            "menu.credits.end.text"           },
};

static ReplayInfo* replays = NULL;
static size_t replay_option = 0;
static float replay_page = 0.f;

static void enter_singleplayer_menu(MenuType), tick_singleplayer_character_menu(), enter_replays_menu(MenuType),
    leave_replays_menu(MenuType), tick_replays_menu(), enter_lobby_list_menu(MenuType), tick_lobby_list_menu(),
    enter_lobby_menu(MenuType), leave_lobby_menu(MenuType), tick_lobby_menu();
static Bool draw_main_menu(), draw_singleplayer_menu(), draw_replays_menu(), draw_lobby_list_menu(), draw_lobby_menu(),
    kick_player_disabled(), start_disabled(), character_disabled();

static const char *fmt_join_code(size_t), *fmt_max_peers(size_t), *fmt_visibility(size_t), *fmt_lobby(),
    *fmt_test_level(size_t);
static void multiplayer_option(), options_option(), exit_option(), max_peers_cycle(Sint8), visibility_cycle(Sint8),
    host_option(), submit_join_code(Bool), character_cycle(Sint8), powerup_cycle(Sint8), enter_as_cycle(Sint8),
    kick_player_option(), promote_player_option(), world_cycle(Sint8), world_option(), start_option(),
    show_copy_join_code_cycle(Sint8), show_copy_join_code_option(), go_to_editor_option(), test_level_cycle(Sint8),
    test_level_option();

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
            .enter = enter_singleplayer_menu,
            .draw = draw_singleplayer_menu,
		},

        [MEN_SINGLEPLAYER_CHARACTER] = {
            .tick = tick_singleplayer_character_menu,
            .draw = draw_singleplayer_menu,
		},

		[MEN_MULTIPLAYER] = {
			.name = "option.multiplayer",
		},

        [MEN_REPLAYS] = {
            .name = "option.replays",
            .enter = enter_replays_menu,
            .leave = leave_replays_menu,
            .tick = tick_replays_menu,
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
			.enter = enter_lobby_menu,
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
            {.name = "option.world", .cycle = world_cycle, .callback = world_option},
        },

        [MEN_SINGLEPLAYER_CHARACTER] = {
            {.name = "option.character", .cycle = character_cycle, .callback = start_option},
            {.name = "option.powerup", .cycle = powerup_cycle, .callback = start_option},
        },

		[MEN_MULTIPLAYER] = {
			{.name = "option.host_lobby", .menu = MEN_HOST_LOBBY},
			{.name = "option.find_lobby", .menu = MEN_LOBBY_LIST},
            {.fmt = fmt_join_code, .prompt = CLIENT.join_code, .prompt_size = sizeof(CLIENT.join_code), .submit = submit_join_code},
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
            {.name = "option.promote", .disabled = kick_player_disabled, .callback = promote_player_option},
            {.name = "option.start", .disabled = start_disabled, .callback = start_option},
#ifdef SDL_PLATFORM_EMSCRIPTEN
            {.name = "option.show_join_code", .cycle = show_copy_join_code_cycle},
#else
            {.name = "option.show_copy_join_code", .cycle = show_copy_join_code_cycle, .callback = show_copy_join_code_option},
#endif
        },

        // GROSS HACK: Add a disabled option in Replays to prevent Select from making noise
        //             (Replays uses a separate system from Catalogs)
        [MEN_REPLAYS] = {
            {.disabled = always_disabled},
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

static float world_description_scroll = 0.f;
static Uint32 character_frame = 0, powerup_frame = 0;

static void enter_singleplayer_menu(MenuType from) {
    (void)from;

    world_description_scroll = 0.f;
    character_frame = powerup_frame = 0;
}

static Bool draw_singleplayer_menu() {
    batch_reset();

    const World* world = get_world(CLIENT.world);
    if (world != NULL) {
        batch_color((CATALOG.current == MEN_SINGLEPLAYER) ? B_U4_WHITE : B_U4_GRAY);
        batch_sprite(fmt("ui/menu/world/%s", world->name));

        const char* lname = LFMT(fmt("world.%s", world->name));
        const float lh = string_height_wrap("header", 32.f, lname, HALF_SCREEN_WIDTH - 64.f) * 0.5f;
        if (CATALOG.current == MEN_SINGLEPLAYER) {
            batch_pos(B_F3_XY(0.f, HALF_SCREEN_HEIGHT - 16.f - lh));
            batch_colors(B_U4X4({255, 255, 255, 50}, {255, 255, 255, 50}, {255, 255, 255, 100}, {255, 255, 255, 100}));
            batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH, lh + 16.f));
            batch_pos(B_F3_XY(0.f, HALF_SCREEN_HEIGHT));
            batch_colors(B_U4X4({255, 255, 255, 100}, {255, 255, 255, 100}, {255, 255, 255, 50}, {255, 255, 255, 50}));
            batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH, lh + 16.f));
        }

        batch_pos(B_F3_XY(HALF_SCREEN_WIDTH * 0.5f, HALF_SCREEN_HEIGHT - 8.f));
        batch_color(B_U4_WHITE);
        batch_align(B_ALIGN_CENTER);
        batch_string_wrap("header", 32.f, lname, HALF_SCREEN_WIDTH - 64.f);
        batch_pos(B_F3_XY(HALF_SCREEN_WIDTH * 0.5f, HALF_SCREEN_HEIGHT - 8.f + lh));
        batch_align(B_ALIGN(FA_CENTER, FA_TOP));
        batch_string("footer", 16.f,
            LFMT((world->num_levels == 1) ? "menu.singleplayer.n_level" : "menu.singleplayer.n_levels", 'u',
                world->num_levels));

        if (CATALOG.current == MEN_SINGLEPLAYER) {
            batch_pos(B_F3_XY(0.f, HALF_SCREEN_HEIGHT));
            batch_flip(B_B2(TRUE, FALSE));
            batch_sprite("ui/menu/singleplayer/arrow");
            batch_pos(B_F3_HALF_SCREEN);
            batch_flip(B_B2(FALSE, FALSE));
            batch_sprite("ui/menu/singleplayer/arrow");

            if (world->description != NULL) {
                batch_pos(B_F3_XY(0.f, SCREEN_HEIGHT - 128.f));
                batch_colors(B_U4X4({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 200}, {0, 0, 0, 200}));
                batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH, 128.f));

                const char* lstr = LFMT(world->description);
                world_description_scroll = SDL_fmodf(world_description_scroll + (deltaticks() * 1.5f),
                    HALF_SCREEN_WIDTH + string_width("footer", 16.f, lstr));

                batch_pos(B_F3_XY(HALF_SCREEN_WIDTH - world_description_scroll, SCREEN_HEIGHT - 64.f));
                batch_color(B_U4_WHITE);
                batch_align(B_ALIGN_BOTTOM_LEFT);
                batch_string("footer", 16.f, lstr);
            }
        }
    }

    batch_pos(B_F3_0);
    batch_sprite("ui/backgrounds/singleplayer");

    const float sx = HALF_SCREEN_WIDTH + (HALF_SCREEN_WIDTH * 0.5f);

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH + 5.f, HALF_SCREEN_HEIGHT - 124.f));
    batch_colors(
        (CATALOG.current == MEN_SINGLEPLAYER_CHARACTER && CATALOG.menus[MEN_SINGLEPLAYER_CHARACTER].option == 0)
            ? B_U4X4({255, 255, 255, 64}, {255, 255, 255, 64}, {255, 255, 255, 128}, {255, 255, 255, 128})
            : B_U4X4({0, 0, 0, 64}, {0, 0, 0, 64}, {0, 0, 0, 32}, {0, 0, 0, 32}));
    batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH - 5.f, 80.f));
    batch_color(B_U4_WHITE);
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT - 136.f));
    batch_sprite(LFMT("menu.lobby.label.character"));
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT - 54.f));
    batch_sprite(get_character_sprite(CLIENT.character, CLIENT.powerup,
        (CATALOG.current == MEN_SINGLEPLAYER_CHARACTER) ? (PF_WALK1 + ((character_frame / 25) % 3)) : PF_IDLE));
    batch_pos(B_F3_XY(sx - 32.f, HALF_SCREEN_HEIGHT - 88.f));
    batch_flip(B_B2(TRUE, FALSE));
    batch_sprite("ui/menu/lobby/arrow");
    batch_pos(B_F3_XY(sx + 32.f, HALF_SCREEN_HEIGHT - 88.f));
    batch_flip(B_B2(FALSE, FALSE));
    batch_sprite("ui/menu/lobby/arrow");
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT - 48.f));
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("footer", 16.f, get_character_name(CLIENT.character));

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH + 5.f, HALF_SCREEN_HEIGHT + 12.f));
    batch_colors(
        (CATALOG.current == MEN_SINGLEPLAYER_CHARACTER && CATALOG.menus[MEN_SINGLEPLAYER_CHARACTER].option == 1)
            ? B_U4X4({255, 255, 255, 64}, {255, 255, 255, 64}, {255, 255, 255, 128}, {255, 255, 255, 128})
            : B_U4X4({0, 0, 0, 64}, {0, 0, 0, 64}, {0, 0, 0, 32}, {0, 0, 0, 32}));
    batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH - 5.f, 48.f));
    batch_color(B_U4_WHITE);
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT));
    batch_sprite(LFMT("menu.lobby.label.powerup"));
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT + 52.f));
    switch (CLIENT.powerup) {
    default: {
        batch_color(B_U4(0, 0, 0, 128));
        batch_sprite("items/mushroom/super");
        batch_color(B_U4_WHITE);

        break;
    }

    case POW_SUPER_MUSHROOM:
        batch_sprite("items/mushroom/super");
        break;
    case POW_FIRE_FLOWER:
        batch_sprite(fmt("items/fire_flower/%u", ((powerup_frame * 27) / 100) % 4));
        break;
    case POW_BEETROOT:
        batch_sprite(fmt("items/beetroot/%u", ((powerup_frame * 2) / 25) % 4));
        break;
    case POW_GREEN_LUI:
        batch_sprite(fmt("items/green_lui/%u", powerup_frame % 12));
        break;
    case POW_HAMMER_SUIT:
        batch_sprite("items/hammer_suit");
        break;
    }
    batch_pos(B_F3_XY(sx - 32.f, HALF_SCREEN_HEIGHT + 28.f));
    batch_flip(B_B2(TRUE, FALSE));
    batch_sprite("ui/menu/lobby/arrow");
    batch_pos(B_F3_XY(sx + 32.f, HALF_SCREEN_HEIGHT + 28.f));
    batch_flip(B_B2(FALSE, FALSE));
    batch_sprite("ui/menu/lobby/arrow");
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT + 56.f));
    batch_string("footer", 16.f, get_powerup_name(CLIENT.powerup));
    batch_pos(B_F3_XY(sx, HALF_SCREEN_HEIGHT + 72.f));
    batch_color(B_U4_ALPHA(192));
    const Sint32 lives = ((world == NULL) ? DEFAULT_LIVES : world->lives) - get_powerup_cost(CLIENT.powerup);
    batch_string(
        "footer", 16.f, LFMT((lives == 1) ? "menu.singleplayer.n_life" : "menu.singleplayer.n_lives", 'i', lives));

    if (CATALOG.current == MEN_SINGLEPLAYER) {
        batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 0.f));
        batch_color(B_U4(0, 0, 0, 128));
        batch_rectangle(NULL, B_F2(HALF_SCREEN_WIDTH, SCREEN_HEIGHT));
    } else {
        batch_color(B_U4_ALPHA(128.f + (SDL_cos(screenticks() * 0.111f) * 128.f)));

        const float sy = HALF_SCREEN_HEIGHT + (HALF_SCREEN_HEIGHT * 0.5f);
        const char *ltop = LFMT("menu.singleplayer.press_to_start.top"), *lmid = kb_label(KB_UI_ENTER),
                   *lbottom = LFMT("menu.singleplayer.press_to_start.bottom");

        for (Uint8 i = 0; i < 2; i++) {
            batch_pos(B_F3_XY(sx, sy - 4.f));
            batch_string("footer", 16.f, ltop);
            batch_pos(B_F3_XY(sx, sy + 28.f));
            batch_align(B_ALIGN_CENTER);
            batch_string("header", 32.f, lmid);
            batch_pos(B_F3_XY(sx, sy + 44.f));
            batch_align(B_ALIGN(FA_CENTER, FA_TOP));
            batch_string("footer", 16.f, lbottom);
        }
    }

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, -155.f));
    batch_colors(B_U4X4({255, 255, 255, 255}, {255, 255, 255, 64}, {255, 255, 255, 0}, {255, 255, 255, 0}));
    batch_circle(NULL, 240.f);
    batch_color(B_U4_WHITE);

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 16.f));
    batch_colors(B_U4X4_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("option.singleplayer"));
    batch_color(B_U4_WHITE);
    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 48.f));
    batch_string("footer", 16.f,
        LFMT((CATALOG.current == MEN_SINGLEPLAYER_CHARACTER) ? "menu.singleplayer.select_character"
                                                             : "menu.singleplayer.select_world"));

    batch_pos(B_F3_XY(-240.f, 0.f));
    batch_colors(B_U4X4({115, 156, 115, 255}, {115, 156, 115, 255}, {189, 231, 189, 255}, {189, 231, 189, 255}));
    batch_rectangle(NULL, B_F2(240.f, SCREEN_HEIGHT));
    batch_pos(B_F3_XY(SCREEN_WIDTH, 0.f));
    batch_rectangle(NULL, B_F2(240.f, SCREEN_HEIGHT));
    batch_pos(B_F3_XY(-5.f, 0.f));
    batch_colors(B_U4X4({0, 0, 0, 0}, {0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 128}));
    batch_rectangle(NULL, B_F2(5.f, SCREEN_HEIGHT));
    batch_pos(B_F3_XY(SCREEN_WIDTH, 0.f));
    batch_colors(B_U4X4({0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 128}, {0, 0, 0, 0}));
    batch_rectangle(NULL, B_F2(5.f, SCREEN_HEIGHT));

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
    batch_color(B_U4_ALPHA(192));
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("footer", 16.f,
        fmt("[%s] %s   [%s/%s] %s   [%s] %s", kb_label(KB_UI_ENTER), LFMT("menu.select"), kb_label(KB_UI_LEFT),
            kb_label(KB_UI_RIGHT), LFMT("menu.cycle"), kb_label(KB_UI_BACK), LFMT("menu.back")));

    return FALSE;
}

static void tick_singleplayer_character_menu() {
    character_frame += 3;
    ++powerup_frame;
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

    if (get_lobby_list_count() <= 0) {
        if (get_lobby_list_state() == LLS_SEARCHING) {
            CATALOG.options[MEN_LOBBY_LIST][0].name = "option.finding_lobbies";
        } else {
            CATALOG.options[MEN_LOBBY_LIST][0].name = "option.no_lobbies";
            lobby_list_hint = TRUE;
        }

        CATALOG.options[MEN_LOBBY_LIST][0].disabled = always_disabled;

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
        lobby_list_refresh = (update_lobby_list() ? 10 : 4) * get_tickrate();
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

static void iterate_replay_file(const char* filename, const void* buffer, size_t size, void* userdata) {
    (void)userdata;

    ReplayInfo replay = {0};

    char* afname = SDL_strdup(filename);
    EXPECT(afname, "Failed to allocate replay \"%s\" file name", filename);

    char* backslash = SDL_strchr(afname, '\\');
    while (backslash != NULL) {
        *backslash = '/';
        backslash = SDL_strchr(backslash, '\\');
    }

    replay.filename = afname;

    replay.name = SDL_strdup(filename_no_ext(file_basename(filename)));
    EXPECT(replay.name, "Failed to allocate replay \"%s\" name", filename);

    SDL_IOStream* io = SDL_IOFromConstMem(buffer, size);
    EXPECT(io, "Failed to stream replay \"%s\"", filename);

    char header[sizeof(REPLAY_HEADER)] = "";
    SDL_ReadIO(io, header, sizeof(header));
    if (SDL_strcmp(header, REPLAY_HEADER) != 0) {
        replay.status = RIS_BAD_HEADER;
        goto bad_replay;
    }

    const Sint64 tell = SDL_TellIO(io);
    replay.version = SDL_strndup((const char*)buffer + tell, size - tell);
    EXPECT(replay.version, "Failed to allocate replay \"%s\" version", filename);
    SDL_SeekIO(io, (Sint64)SDL_strlen(replay.version) + 1, SDL_IO_SEEK_CUR);

    if (SDL_strcmp(replay.version, GAME_VERSION) != 0) {
        replay.status = RIS_BAD_VERSION;
        goto bad_replay;
    }

    SDL_ReadU32LE(io, &replay.checksum);
    if (replay.checksum != get_game_hash()) {
        replay.status = RIS_BAD_CHECKSUM;
        goto bad_replay;
    }

    SDL_SeekIO(io, sizeof(Sint8), SDL_IO_SEEK_CUR);
    SDL_ReadU64LE(io, &replay.level);
    SDL_SeekIO(io, (sizeof(Uint64) * 2) + sizeof(Uint16) + sizeof(Sint16), SDL_IO_SEEK_CUR);
    SDL_ReadS8(io, &replay.num_players);

bad_replay:
    replays = TinyDPush((replays == NULL) ? MakeTinyDPro(1, sizeof(ReplayInfo)) : replays, &replay);
}

static void enter_replays_menu(MenuType from) {
    (void)from;

    leave_replays_menu(MEN_NULL);
    iterate_user_files("replays/*.rpl", TRUE, iterate_replay_file, NULL);
    if (replay_option > TinyDLength(replays)) {
        replay_option = 0;
        replay_page = 0.f;
    }
}

static void leave_replays_menu(MenuType to) {
    (void)to;

    for (size_t i = 0, n = TinyDLength(replays); i < n; i++) {
        SDL_free((void*)replays[i].filename);
        SDL_free((void*)replays[i].name);
        SDL_free((void*)replays[i].version);
    }

    FreeTinyD(replays);
    replays = NULL;
}

static void tick_replays_menu() {
    const size_t num_replays = TinyDLength(replays);
    if (num_replays <= 0)
        return;

    const Sint8 change = (Sint8)((Sint8)kb_pressed(KB_UI_DOWN) - (Sint8)kb_pressed(KB_UI_UP)),
                crepeat = (Sint8)((Sint8)kb_repeated(KB_UI_DOWN) - (Sint8)kb_repeated(KB_UI_UP)),
                ctotal = (Sint8)((Sint8)change + (Sint8)crepeat);
    if (ctotal != 0) {
        const size_t last_option = replay_option;

        if (ctotal < 0) {
            if (replay_option <= 0)
                replay_option = num_replays - 1;
            else
                --replay_option;
        } else if (ctotal > 0) {
            if (replay_option >= (num_replays - 1))
                replay_option = 0;
            else
                ++replay_option;
        }

        const float y = (float)replay_option * 18.f, page_y = y - replay_page;
        if (last_option < replay_option && page_y > SCREEN_HEIGHT - 50.f)
            replay_page = y - (SCREEN_HEIGHT - 50.f);
        else if (last_option > replay_option && page_y < 0.f)
            replay_page = y;

        play_generic_sound((crepeat == 0) ? "ui/switch" : "ui/repeat", 0);
    }

    if (kb_pressed(KB_UI_ENTER) && replay_option < num_replays && replays[replay_option].status == RIS_OK) {
        load_replay(replays[replay_option].filename);
        play_generic_sound("ui/select", 0);
    }
}

static Bool draw_replays_menu() {
    batch_reset();
    batch_colors(B_U4X4({115, 156, 115, 255}, {115, 156, 115, 255}, {189, 231, 189, 255}, {189, 231, 189, 255}));
    batch_rectangle(NULL, B_F2(240.f, SCREEN_HEIGHT));
    batch_pos(B_F3_XY(-5.f, 0.f));
    batch_colors(B_U4X4({0, 0, 0, 0}, {0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 128}));
    batch_rectangle(NULL, B_F2(5.f, SCREEN_HEIGHT));
    batch_pos(B_F3_XY(240.f, 0.f));
    batch_colors(B_U4X4({0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 128}, {0, 0, 0, 0}));
    batch_rectangle(NULL, B_F2(5.f, SCREEN_HEIGHT));

    const size_t num_replays = TinyDLength(replays);
    for (size_t i = 0; i < num_replays; i++) {
        const ReplayInfo* replay = &replays[i];

        batch_pos(B_F3_XY(256.f, 17.f - replay_page + (i * 18.f)));
        batch_color((replay->status == RIS_OK) ? B_U4_WHITE : B_U4_RED);
        batch_string("footer", 16.f, replay->name);
    }

    batch_pos(B_F3_XY(120.f, 16.f));
    batch_colors(B_U4X4_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("option.replays"));

    batch_color(B_U4_WHITE);
    if (num_replays <= 0) {
        batch_pos(B_F3_XY(440.f, HALF_SCREEN_HEIGHT));
        batch_align(B_ALIGN_CENTER);
        batch_string_wrap("footer", 16.f, LFMT("menu.replays.no_replays", 's', kb_label(KB_RECORD_REPLAY)), 400.f);
    } else if (replay_option < num_replays) {
        batch_blend(BM_ADD);
        batch_pos(B_F3_XY(240.f, 16.f - replay_page + (replay_option * 18.f)));
        batch_color(B_U4_VALUE(32));
        batch_rectangle(NULL, B_F2(1000.f, 18.f));
        batch_color(B_U4_WHITE);
        batch_blend(BM_NORMAL);

        const ReplayInfo* replay = &replays[replay_option];

        float y = 56.f;
        batch_pos(B_F3_XY(16.f, y));
        batch_align(B_ALIGN_TOP_LEFT);
        batch_string_wrap("footer", 16.f, replay->name, 208.f);

        y += string_height_wrap("footer", 16.f, replay->name, 208.f);
        batch_pos(B_F3_XY(16.f, y));
        batch_color(B_U4_ALPHA(192));
        batch_string_wrap("footer", 12.f, replay->filename, 208.f);

        y += string_height_wrap("footer", 12.f, replay->filename, 208.f) + 8.f;
        batch_pos(B_F3_XY(16.f, y));
        batch_color(B_U4_WHITE);

        switch (replay->status) {
        default: {
            const Level* level = get_level_key(replay->level);
            batch_string_wrap("footer", 16.f,
                fmt("%s\n%s", (level == NULL) ? "(null)" : LFMT(fmt("level.%s", level->name)),
                    (replay->num_players <= 1) ? LFMT("menu.replays.singleplayer")
                                               : LFMT("menu.replays.n_players", 'i', replay->num_players)),
                208.f);

            break;
        }

        case RIS_BAD_HEADER:
            batch_string_wrap("footer", 16.f, LFMT("menu.replays.bad_header"), 208.f);
            break;
        case RIS_BAD_VERSION:
            batch_string_wrap("footer", 16.f,
                fmt("%s\n\n%s: %s\n%s: %s", LFMT("menu.replays.bad_version"), LFMT("menu.replays.replay_version"),
                    replay->version, LFMT("menu.replays.your_version"), GAME_VERSION),
                208.f);
            break;
        case RIS_BAD_CHECKSUM:
            batch_string_wrap("footer", 16.f,
                fmt("%s\n\n%s: %u\n%s: %u", LFMT("menu.replays.bad_checksum"), LFMT("menu.replays.replay_checksum"),
                    replay->checksum, LFMT("menu.replays.your_checksum"), get_game_hash()),
                208.f);
            break;
        }
    }

    batch_pos(B_F3_XY(120.f, SCREEN_HEIGHT - 16.f));
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string_wrap("footer", 16.f,
        (num_replays > 0 && replay_option < num_replays && replays[replay_option].status == RIS_OK)
            ? fmt("[%s] %s\n[%s] %s", kb_label(KB_UI_ENTER), LFMT("menu.play"), kb_label(KB_UI_BACK), LFMT("menu.back"))
            : fmt("[%s] %s", kb_label(KB_UI_BACK), LFMT("menu.back")),
        208.f);

    return FALSE;
}

static const char* fmt_lobby() {
    const char* lname = get_lobby_name();
    return fmt("%s (%s)", (lname != NULL && SDL_strnlen(lname, 33) > 32) ? fmt("%.*s...", 32, lname) : lname,
        LFMT(in_private_lobby() ? "value.private" : "value.public"));
}

static const char* fmt_disconnected() {
    const char* error = net_error();
    return (error == NULL) ? LFMT("message.disconnected") : fmt("%s\n(%s)", LFMT("message.disconnected"), error);
}

static void cancel_error() {
    previous_menu(&CATALOG);
}

#ifndef SDL_PLATFORM_EMSCRIPTEN
static Uint8 lobby_copy_time = 0;
#endif

static void enter_lobby_menu(MenuType from) {
    (void)from;

    CLIENT.show_join_code = FALSE;
#ifndef SDL_PLATFORM_EMSCRIPTEN
    lobby_copy_time = 0;
#endif
}

static void leave_lobby_menu(MenuType to) {
    (void)to;

    disconnect();
    play_generic_sound("ui/disconnect", PLAY_SYSTEM);
}

static void tick_lobby_menu() {
#ifndef SDL_PLATFORM_EMSCRIPTEN
    if (lobby_copy_time > 0)
        --lobby_copy_time;
#endif

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

static void draw_lobby_code(float* y) {
    const float y1 = *y;

    batch_pos(B_F3_XY(125.f, *y));
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));

    const char *font = "footer", *str = NULL;
    float size = 16.f, wrap = 186.f;

#ifndef SDL_PLATFORM_EMSCRIPTEN
    if (lobby_copy_time > 0) {
        str = LFMT("option.copied");
    } else
#endif
        if (CLIENT.show_join_code)
    {
        font = "header";
        size = 22.f;
        str = u64_to_base32(get_lobby_id());
        wrap = 224.f;
    } else {
        str = LFMT(CATALOG.options[MEN_LOBBY][8].name);
    }

    if (size >= 17.f) {
        batch_offset(B_F3_XY(0.f, 4.f));
        batch_string_wrap(font, size, str, wrap);
        batch_string_wrap(font, size, str, wrap);
        batch_offset(B_F3_0);
    } else {
        batch_string_wrap(font, size, str, wrap);
    }

    const float hw = string_width_wrap(font, size, str, wrap) * 0.5f;
    batch_pos(B_F3_XY(115.f - hw, *y));
    batch_flip(B_B2(TRUE, FALSE));
    batch_sprite("ui/menu/lobby/arrow");
    batch_pos(B_F3_XY(135.f + hw, *y));
    batch_flip(B_B2_FALSE);
    batch_sprite("ui/menu/lobby/arrow");

    *y += string_height_wrap(font, size, str, wrap) + 22.f - size;

    if (CATALOG.menus[MEN_LOBBY].option == 8) {
        batch_pos(B_F3_XY(-240.f, y1 - 4.f));
        batch_colors(B_U4X4({0, 0, 0, 255}, {40, 40, 40, 255}, {0, 0, 0, 255}, {40, 40, 40, 255}));
        batch_blend(BM_ADD);
        batch_rectangle(NULL, B_F2(490.f, *y - y1));
        batch_blend(BM_NORMAL);
    }

    *y += 1.f;
}

static void draw_lobby_cycle(float* y, size_t idx, const char* label, const char* value, Bool disabled) {
    const float y1 = *y;

    batch_pos(B_F3_XY(125.f, *y));
    batch_color(B_U4_ALPHA(disabled ? 160 : 255));
    batch_sprite(LFMT(label));

    *y += 19.f;
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

    *y += 1.f;
}

static const char* fmt_lobby_world(size_t idx) {
    (void)idx;

    const World* world = get_world(is_connected() ? get_lobby_string("world") : CLIENT.world);
    return (world == NULL) ? NULL : LFMT(fmt("world.%s", world->name));
}

static const char* fmt_lobby_powerup(size_t idx) {
    (void)idx;

    const Sint32 cost = get_powerup_cost(CLIENT.powerup);
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

    *y += 47.f;
}

static Bool draw_lobby_menu() {
    batch_reset();

    // LEFT
    batch_pos(B_F3_XY(-240.f, 0.f));
    batch_colors(B_U4X4({115, 156, 115, 255}, {115, 156, 115, 255}, {189, 231, 189, 255}, {189, 231, 189, 255}));
    const char* lname = fmt("%s (%s)", get_lobby_name(), LFMT(in_private_lobby() ? "value.private" : "value.public"));
    const float lh = string_height_wrap("footer", 16.f, lname, 218.f);
    batch_rectangle(NULL, B_F2(490.f, 6.f + lh));
    batch_pos(B_F3_XY(-240.f, 6.f + lh));
    batch_colors(B_U4X4({0, 0, 0, 128}, {0, 0, 0, 128}, {0, 0, 0, 0}, {0, 0, 0, 0}));
    batch_rectangle(NULL, B_F2(490.f, 2.f));
    batch_pos(B_F3_XY(125.f, 3.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string_wrap("footer", 16.f, lname, 230.f);

    float ly = lh + 12.f;

    draw_lobby_code(&ly);

    draw_lobby_cycle(&ly, 0, "menu.lobby.label.world", fmt_lobby_world(0), is_client());
    draw_lobby_cycle(&ly, 1, "menu.lobby.label.enter_as",
        LFMT(get_peer_bool(get_local_peer(), "spectator") ? "value.spectator" : "value.player"), FALSE);
    draw_lobby_cycle(&ly, 2, "menu.lobby.label.character", get_character_name(CLIENT.character), character_disabled());
    draw_lobby_cycle(&ly, 3, "menu.lobby.label.powerup", fmt_lobby_powerup(0), character_disabled());

    draw_lobby_button(&ly, 4, LFMT("menu.lobby.button.options"), FALSE);
    if (is_host()) {
        const Bool disabled = kick_player_disabled();
        draw_lobby_button(&ly, 5, LFMT("menu.lobby.button.kick"), disabled);
        draw_lobby_button(&ly, 6, LFMT("menu.lobby.button.promote"), disabled);
    }
    draw_lobby_button(&ly, 7,
        LFMT(get_lobby_player_count() >= 1 ? (is_client() ? "menu.lobby.button.waiting_for_host"
                                                          : (all_peers_ready() ? "menu.lobby.button.start"
                                                                               : "menu.lobby.button.waiting_for_peers"))
                                           : "menu.lobby.button.not_enough_players"),
        start_disabled());

    batch_pos(B_F3_XY(125.f, SCREEN_HEIGHT - 6.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    const char* ind = fmt("[%s] %s", kb_label(KB_UI_BACK), LFMT("menu.disconnect"));
    batch_string_wrap("footer", 16.f, ind, 218.f);

    batch_pos(B_F3_XY(125.f, SCREEN_HEIGHT - 9.f - string_height_wrap("footer", 16.f, ind, 218.f)));
    batch_color(B_U4_ALPHA(200));
    batch_string_wrap("footer", 12.f, fmt("Checksum: %u", get_game_hash()), 218.f);

    // RIGHT
    const Sint32 lives = get_world_lives(get_world(get_lobby_string("world")));

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

        const Bool connected = peer_is_ready(pid), spectating = get_peer_bool(pid, "spectator");
        batch_pos(B_F3_XY(295.f, ly + 27.f));
        batch_color(B_U4_ALPHA((!connected || spectating) ? 100 : 255));
        batch_sprite(fmt(get_character_cursor(get_peer_number(pid, "character")), 0));

        batch_pos(B_F3_XY(317.f, ly + 8.f));
        batch_color((get_master_peer() == pid) ? B_U4_YELLOW : B_U4_WHITE);
        batch_align(B_ALIGN_TOP_LEFT);
        const char* name = get_peer_name(pid);
        batch_string("footer", 16.f, name);

        if (!connected) {
            batch_pos(B_F3_XY(317.f, ly + 28.f));
            batch_color(B_U4_ALPHA(128));
            batch_string("footer", 16.f, LFMT("value.connecting"));
        } else if (spectating) {
            batch_pos(B_F3_XY(317.f, ly + 28.f));
            batch_color(B_U4_ALPHA(200));
            batch_string("footer", 16.f, LFMT("value.spectator"));
        } else {
            const PlayerPowerup powerup = get_peer_number(pid, "powerup");
            batch_pos(B_F3_XY(322.f + string_width("footer", 16.f, name), ly + 8.f));
            batch_color(B_U4_ALPHA(200));
            batch_string("footer", 16.f, fmt("x %i", lives - get_powerup_cost(powerup)));

            batch_pos(B_F3_XY(317.f, ly + 28.f));
            batch_color((powerup == POW_NONE) ? B_U4_ALPHA(128) : B_U4_WHITE);
            batch_string("footer", 16.f, LFMT((powerup == POW_NONE) ? "value.no_powerup" : get_powerup_name(powerup)));
        }

        batch_color(B_U4_WHITE);

        if (peer_is_relayed(pid)) {
            batch_pos(B_F3_XY(610.f, ly + 28.f));
            batch_sprite("ui/menu/lobby/relay");

            batch_pos(B_F3_XY(590.f, ly + 28.f));
        } else {
            batch_pos(B_F3_XY(610.f, ly + 28.f));
        }

        batch_align(B_ALIGN(FA_RIGHT, FA_MIDDLE));
        batch_string("footer", 16.f, connected ? fmt("%i ms", get_peer_ping(pid)) : "...");

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

    world_description_scroll = 0.f;
}

static void world_option() {
    set_menu(&CATALOG, MEN_SINGLEPLAYER_CHARACTER);
    play_generic_sound("ui/ready", PLAY_POS);
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
    return is_client() || get_world(CLIENT.world) == NULL || get_lobby_player_count() < 1 || !all_peers_ready();
}

static void start_option() {
    if (is_client())
        return;

    const World* world = get_world(CLIENT.world);
    if (world == NULL)
        return;

    const WorldSaveInfo save = world_has_save(world);
    if (save.valid) {
        UI* ui = create_ui(UI_SAVE, NULL);
        if (ui != NULL) {
            *(WorldSaveInfo*)ui->userdata = save;
            return;
        }
    }

    if (is_connected())
        peers_to_players();

    WorldContext ctx = init_world_context(world);
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

static const char* fmt_join_code(size_t idx) {
    (void)idx;

    return (typing_what() == CLIENT.join_code)
               ? fmt("%s: %s%s", LFMT("option.join_code"), CLIENT.join_code, caret(TRUE))
               : LFMT("option.join_via_code");
}

static void submit_join_code(Bool confirmed) {
    if (confirmed) {
        const NetID lid = base32_to_u64(CLIENT.join_code);
        if (lid > 0) {
            join_lobby(lid);
            prompt_connect();
        }
    }

    CLIENT.join_code[0] = '\0';
}

static void lobby_option() {
    const LobbyInfo* lobby = get_lobby_list(CATALOG.menus[MEN_LOBBY_LIST].option);
    if (lobby != NULL) {
        join_lobby(lobby->id);
        prompt_connect();
    }
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

static void promote_player_option() {
    UI* promote = create_ui(UI_KICK, NULL);
    if (promote != NULL)
        ((UIKickData*)promote->userdata)->promote = TRUE;
}

static void show_copy_join_code_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.show_join_code = !CLIENT.show_join_code;
#ifndef SDL_PLATFORM_EMSCRIPTEN
    lobby_copy_time = 0;
#endif
}

static void show_copy_join_code_option() {
#ifndef SDL_PLATFORM_EMSCRIPTEN
    if (copy_to_clipboard(u64_to_base32(get_lobby_id())))
        lobby_copy_time = (3 * get_tickrate()) / 2;
#endif
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
    WorldContext wctx = init_world_context(NULL);
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
    load_sprite("ui/backgrounds/singleplayer", AKL_NEVER);
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
    load_sprite("ui/menu/singleplayer/arrow", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.world", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.enter_as", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.character", AKL_NEVER);
    load_localized_sprite("menu.lobby.label.powerup", AKL_NEVER);
    load_sprite("ui/menu/lobby/arrow", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.options", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.kick", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.promote", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.start", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.not_enough_players", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.waiting_for_host", AKL_NEVER);
    load_localized_sprite("menu.lobby.button.waiting_for_peers", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/empty", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/peer", AKL_NEVER);
    load_sprite("ui/menu/lobby/slot/you", AKL_NEVER);
    load_sprite("ui/menu/lobby/relay", AKL_NEVER);
    load_font("menu", AKL_NEVER);
    load_sound("ui/ready", AKL_NEVER);
    load_sound("ui/enter", AKL_ONCE);
    load_sound("ui/connect", AKL_NEVER);
    load_sound("ui/disconnect", AKL_NEVER);
    load_track("title", AKL_NEVER);

    const World* world = NULL;
    world_iterate_start();
    while ((world = world_iterate_next()))
        load_sprite(fmt("ui/menu/world/%s", world->name), AKL_NEVER);

    for (PlayerCharacter i = 0; i < (PlayerCharacter)CHR_SIZE; i++) {
        for (PlayerPowerup j = 0; j < (PlayerPowerup)POW_SIZE; j++) {
            load_sprite(get_character_sprite(i, j, PF_IDLE), AKL_NEVER);
            load_sprite(get_character_sprite(i, j, PF_WALK1), AKL_NEVER);
            load_sprite(get_character_sprite(i, j, PF_WALK2), AKL_NEVER);
            load_sprite(get_character_sprite(i, j, PF_WALK3), AKL_NEVER);
        }
    }

    load_sprite("items/mushroom/super", AKL_NEVER);
    load_sprite_num("items/fire_flower/%u", 4, AKL_NEVER);
    load_sprite_num("items/beetroot/%u", 4, AKL_NEVER);
    load_sprite_num("items/green_lui/%u", 12, AKL_NEVER);
    load_sprite("items/hammer_suit", AKL_NEVER);

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
    if (((got_invite || !is_connected()) && CATALOG.current == MEN_LOBBY)
        || CATALOG.current == MEN_SINGLEPLAYER_CHARACTER)
    {
        previous_menu(&CATALOG);
    }

    if (last_menu == CATALOG.current && CATALOG.current > MEN_NULL && CATALOG.current < MEN_SIZE) {
        Menu* menu = &CATALOG.menus[CATALOG.current];
        if (menu->enter != NULL)
            menu->enter(menu->from);
    }

    play_generic_track("title", PLAY_LOOPING, 0);
    fade_generic_track(1.f, 100.f);
}

static void end() {
    leave_replays_menu(MEN_NULL); // GROSS HACK: Make sure to free replay list when leaving this screen.
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
