#include "K_audio.h"
#include "K_cmd.h"
#include "K_config.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_video.h"

enum {
    MEN_NULL,

    MEN_MAIN,
    MEN_LANGUAGE,
    MEN_CONTROLS,
    MEN_VIDEO,
    MEN_AUDIO,
    MEN_NETWORK,

    MEN_SIZE,
};

static const char *fmt_language(size_t), *fmt_name(size_t), *fmt_server(size_t), *fmt_show_user_messages(size_t),
    *fmt_language_option(size_t), *fmt_resolution(size_t), *fmt_fullscreen(size_t), *fmt_vsync(size_t),
    *fmt_master_volume(size_t), *fmt_sound_volume(size_t), *fmt_music_volume(size_t), *fmt_audio_in_background(size_t),
    *fmt_input_delay(size_t), *fmt_device(size_t), *fmt_up(size_t), *fmt_left(size_t), *fmt_down(size_t),
    *fmt_right(size_t), *fmt_jump(size_t), *fmt_run(size_t), *fmt_fire(size_t), *fmt_chat(size_t),
    *fmt_record_replay(size_t), *fmt_framerate(size_t), *fmt_texture_filter(size_t);
static void enter_language_menu(MenuType), submit_name(Bool), submit_server(Bool), show_user_messages_cycle(Sint8),
    language_option(), resolution_cycle(Sint8), fullscreen_cycle(Sint8), master_volume_cycle(Sint8),
    sound_volume_cycle(Sint8), music_volume_cycle(Sint8), audio_in_background_cycle(Sint8), vsync_cycle(Sint8),
    input_delay_cycle(Sint8), up_option(), left_option(), down_option(), right_option(), jump_option(), run_option(),
    fire_option(), chat_option(), record_replay_option(), framerate_cycle(Sint8), texture_filter_cycle(Sint8);

static Catalog CATALOG = {
	.current = MEN_MAIN,

	.menus = {
		[MEN_MAIN] = {.name = "opt_options"},
		[MEN_LANGUAGE] = {.name = "opt_language", .enter = enter_language_menu},
		[MEN_CONTROLS] = {.name = "opt_controls"},
		[MEN_VIDEO] = {.name = "opt_video"},
		[MEN_AUDIO] = {.name = "opt_audio"},
        [MEN_NETWORK] = {.name = "opt_network"},
	},

	.options = {
		[MEN_MAIN] = {
            {.fmt = fmt_name, OPTION_PROMPT(CLIENT.name), .submit = submit_name},
			{.fmt = fmt_language, .menu = MEN_LANGUAGE},
			{},
			{.name = "opt_controls", .menu = MEN_CONTROLS},
			{.name = "opt_video", .menu = MEN_VIDEO},
			{.name = "opt_audio", .menu = MEN_AUDIO},
            {.name = "opt_network", .menu = MEN_NETWORK},
		},

		[MEN_CONTROLS] = {
            {.fmt = fmt_device, .disabled = always_disabled},
            {.fmt = fmt_up, .callback = up_option},
            {.fmt = fmt_left, .callback = left_option},
            {.fmt = fmt_down, .callback = down_option},
            {.fmt = fmt_right, .callback = right_option},
            {.fmt = fmt_jump, .callback = jump_option},
            {.fmt = fmt_run, .callback = run_option},
            {.fmt = fmt_fire, .callback = fire_option},
            {},
            {.fmt = fmt_chat, .callback = chat_option},
            {.fmt= fmt_record_replay, .callback = record_replay_option},
		},

		[MEN_VIDEO] = {
			{.fmt = fmt_resolution, .cycle = resolution_cycle},
            {.fmt = fmt_framerate, .cycle = framerate_cycle},
            {},
			{.fmt = fmt_fullscreen, .cycle = fullscreen_cycle},
#ifndef SDL_PLATFORM_EMSCRIPTEN
			{.fmt = fmt_vsync, .cycle = vsync_cycle},
#endif
            {},
            {.fmt = fmt_texture_filter, .cycle = texture_filter_cycle},
		},

		[MEN_AUDIO] = {
			{.fmt = fmt_master_volume, .cycle = master_volume_cycle},
			{.fmt = fmt_sound_volume, .cycle = sound_volume_cycle},
			{.fmt = fmt_music_volume, .cycle = music_volume_cycle},
            {},
            {.fmt = fmt_audio_in_background, .cycle = audio_in_background_cycle},
		},

        [MEN_NETWORK] = {
            {.fmt = fmt_input_delay, .cycle = input_delay_cycle},
            {.fmt = fmt_show_user_messages, .cycle = show_user_messages_cycle},
            {},
            {.fmt = fmt_server, OPTION_PROMPT(CLIENT.server), .submit = submit_server},
        }
	}
};

// =====
// MENUS
// =====

static void enter_language_menu(MenuType from) {
    (void)from;

    SDL_zeroa(CATALOG.options[MEN_LANGUAGE]);

    size_t i = 0;
    const Language* language = NULL;
    language_iterate_start();
    while (i < MAX_OPTIONS && (language = language_iterate_next())) {
        Option* option = &CATALOG.options[MEN_LANGUAGE][i];
        option->name = language->name;
        option->fmt = fmt_language_option;
        option->callback = language_option;
        i++;
    }
}

// =======
// OPTIONS
// =======

static const char* fmt_name(size_t idx) {
    (void)idx;

    return fmt("%s: %s%s", LFMT("opt_name"), CLIENT.name, caret(typing_what() == CLIENT.name));
}

static void submit_name(Bool confirmed) {
    (void)confirmed;

    push_user_data();
}

static const char* fmt_language(size_t idx) {
    (void)idx;

    return fmt("%s: %s (%s)", LFMT("opt_language"), LFMT(CLIENT.language), CLIENT.language);
}

static const char* fmt_server(size_t idx) {
    (void)idx;

    return (typing_what() == CLIENT.server) ? fmt("%s:\n%s%s", LFMT("opt_server"), CLIENT.server, caret(TRUE))
                                            : LFMT("opt_change_server");
}

static void submit_server(Bool confirmed) {
    (void)confirmed;

    set_hostname(CLIENT.server);
}

static const char* fmt_show_user_messages(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_show_user_messages"), LFMT(CLIENT.show_user_messages ? "val_yes" : "val_no"));
}

static void show_user_messages_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.show_user_messages = !CLIENT.show_user_messages;
}

static const char* fmt_language_option(size_t idx) {
    const char* lname = CATALOG.options[MEN_LANGUAGE][idx].name;
    return fmt("%s (%s)", LFMT(lname), lname);
}

static void language_option() {
    const char* lname = CATALOG.options[MEN_LANGUAGE][CATALOG.menus[MEN_LANGUAGE].option].name;
    SDL_strlcpy(CLIENT.language, lname, sizeof(CLIENT.language));
    apply_language(CLIENT.language);
}

static const char* fmt_device(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_device"), input_device());
}

#define KEYBIND_OPTION(name, kb)                                                                                       \
    static const char* fmt_##name(size_t idx) {                                                                        \
        (void)idx;                                                                                                     \
                                                                                                                       \
        return fmt("%s: %s", LFMT("kb_" #name), (scanning_what() == (kb)) ? LFMT("val_scanning") : kb_label(kb));      \
    }                                                                                                                  \
                                                                                                                       \
    static void name##_option() {                                                                                      \
        start_scanning(kb);                                                                                            \
    }

KEYBIND_OPTION(up, KB_UP);
KEYBIND_OPTION(left, KB_LEFT);
KEYBIND_OPTION(down, KB_DOWN);
KEYBIND_OPTION(right, KB_RIGHT);
KEYBIND_OPTION(jump, KB_JUMP);
KEYBIND_OPTION(run, KB_RUN);
KEYBIND_OPTION(fire, KB_FIRE);
KEYBIND_OPTION(chat, KB_CHAT);
KEYBIND_OPTION(record_replay, KB_RECORD_REPLAY);

#undef KEYBIND_OPTION

static const char* fmt_resolution(size_t idx) {
    (void)idx;

    int width = 0, height = 0;
    get_resolution(&width, &height);
    return fmt("%s: %ix%i", LFMT("opt_resolution"), width, height);
}

static void resolution_cycle(Sint8 cycle) {
    int width = 0, height = 0;
    get_resolution(&width, &height);

    const float sx = (float)width / (float)SCREEN_WIDTH;
    const float sy = (float)height / (float)SCREEN_HEIGHT;
    const float scale = SDL_max(sx, sy);

    if (cycle > 0) {
        if (scale < 1.25f)
            set_resolution(SCREEN_WIDTH + HALF_SCREEN_WIDTH, SCREEN_HEIGHT + HALF_SCREEN_HEIGHT, TRUE);
        else if (scale >= 1.25f && scale < 1.75f)
            set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, TRUE);
        else if (scale >= 1.75f)
            set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
    } else if (cycle < 0) {
        if (scale < 1.25f)
            set_resolution(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, TRUE);
        else if (scale >= 1.25f && scale < 1.75f)
            set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
        else if (scale >= 1.75f)
            set_resolution(SCREEN_WIDTH + HALF_SCREEN_WIDTH, SCREEN_HEIGHT + HALF_SCREEN_HEIGHT, TRUE);
    }
}

static const char* fmt_fullscreen(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_fullscreen"), LFMT(get_fullscreen() ? "val_on" : "val_off"));
}

static void fullscreen_cycle(Sint8 cycle) {
    (void)cycle;

    set_fullscreen(!get_fullscreen());
}

static const char* fmt_vsync(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_vsync"), LFMT(get_vsync() ? "val_on" : "val_off"));
}

static void vsync_cycle(Sint8 cycle) {
    (void)cycle;

    set_vsync(!get_vsync());
}

static const char* fmt_master_volume(size_t idx) {
    (void)idx;

    return fmt("%s: %u%%", LFMT("opt_master_volume"), (Uint8)(get_volume() * 100.f));
}

static void master_volume_cycle(Sint8 cycle) {
    Sint32 percent = (Sint32)(get_volume() * 100.f) + (5 * cycle);
    if (percent > 100)
        percent = 0;
    else if (percent < 0)
        percent = 100;

    set_volume((float)percent / 100.f);
}

static const char* fmt_sound_volume(size_t idx) {
    (void)idx;

    return fmt("%s: %u%%", LFMT("opt_sound_volume"), (Uint8)(get_sound_volume() * 100.f));
}

static void sound_volume_cycle(Sint8 cycle) {
    Sint32 percent = (Sint32)(get_sound_volume() * 100.f) + (5 * cycle);
    if (percent > 100)
        percent = 0;
    else if (percent < 0)
        percent = 100;

    set_sound_volume((float)percent / 100.f);
}

static const char* fmt_music_volume(size_t idx) {
    (void)idx;

    return fmt("%s: %u%%", LFMT("opt_music_volume"), (Uint8)(get_music_volume() * 100.f));
}

static void music_volume_cycle(Sint8 cycle) {
    Sint32 percent = (Sint32)(get_music_volume() * 100.f) + (5 * cycle);
    if (percent > 100)
        percent = 0;
    else if (percent < 0)
        percent = 100;

    set_music_volume((float)percent / 100.f);
}

static const char* fmt_audio_in_background(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_audio_in_background"), LFMT(CLIENT.audio_in_background ? "val_on" : "val_off"));
}

static void audio_in_background_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.audio_in_background = !CLIENT.audio_in_background;
}

static const char* fmt_input_delay(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_input_delay"),
        LFMT((CLIENT.input_delay == 1) ? "val_frame" : "val_frames", 'd', CLIENT.input_delay));
}

static void input_delay_cycle(Sint8 cycle) {
    if (cycle > 0) {
        CLIENT.input_delay = (CLIENT.input_delay + 1) % (MAX_INPUT_DELAY + 1);
    } else if (cycle < 0) {
        if (CLIENT.input_delay <= 0)
            CLIENT.input_delay = MAX_INPUT_DELAY;
        else
            --CLIENT.input_delay;
    }
}

static const char* fmt_framerate(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_framerate"),
        (get_framerate() <= 0) ? LFMT("val_unlimited") : LFMT("val_fps", 'd', get_framerate()));
}

static void framerate_cycle(Sint8 cycle) {
    (void)cycle;

    static const int ranges[] = {0, 30, 50, 60, 75, 120, 144, 165, 180, 240, 360, 480};
    const size_t len = SDL_arraysize(ranges);

    const int fps = get_framerate();
    if (cycle >= 0) {
        for (int i = 0; i < len; i++) {
            if (fps < ranges[i]) {
                set_framerate(ranges[i]);
                return;
            }
        }
        set_framerate(ranges[0]);
    } else {
        for (int i = (int)len - 2; i >= 0; i--) {
            if (fps > ranges[i]) {
                set_framerate(ranges[i]);
                return;
            }
        }
        set_framerate(ranges[len - 1]);
    }
}

static const char* fmt_texture_filter(size_t idx) {
    (void)idx;

    return fmt("%s: %s", LFMT("opt_texture_filter"), LFMT(CLIENT.texture_filter ? "val_linear" : "val_nearest"));
}

static void texture_filter_cycle(Sint8 cycle) {
    (void)cycle;

    CLIENT.texture_filter = !CLIENT.texture_filter;
}

// ==
// UI
// ==

static void create(UI* ui) {
    ui->flags |= UIF_BLOCK;
}

static void tick(UI* ui) {
    if (!tick_catalog(&CATALOG, ui))
        ui->flags |= UIF_DESTROY;
}

static void draw(const UI* ui) {
    (void)ui;

    if (get_screen() != SCR_MENU) {
        batch_reset();
        batch_color(B_RGBA(0, 0, 0, 128));
    }

    draw_catalog(&CATALOG);
}

static void cleanup(UI* ui) {
    (void)ui;

    save_config();
}

const UITable TAB_OPTIONS = {
    .create = create,
    .tick = tick,
    .draw = draw,
    .cleanup = cleanup,
};
