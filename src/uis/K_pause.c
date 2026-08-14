#include "K_audio.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_video.h"

static Option OPTIONS[MAX_OPTIONS] = {0};
static size_t option = 0;

static void options_option() {
    create_ui(UI_OPTIONS, topui());
}

static Bool kick_player_disabled() {
    return !is_connected() || is_client() || get_peer_count() <= 1;
}

static void kick_player_option() {
    create_ui(UI_KICK, topui());
}

static const char* fmt_return_to_title(size_t idx) {
    (void)idx;

    return LFMT(is_connected() ? "option.return_to_lobby" : "option.return_to_title");
}

static void return_to_title_option() {
    bail_from_game();
    set_screen(SCR_MENU, NULL, 0);
}

static void load() {
    load_sound("ui/pause", AKL_NEVER);
}

static void create(UI* ui) {
    ui->flags |= UIF_BLOCK;

    SDL_zeroa(OPTIONS);
    if (is_connected() && is_host()) {
        OPTIONS[0].name = "option.options";
        OPTIONS[0].callback = options_option;
        OPTIONS[1].name = "option.kick_player";
        OPTIONS[1].disabled = kick_player_disabled;
        OPTIONS[1].callback = kick_player_option;
        OPTIONS[2].fmt = fmt_return_to_title;
        OPTIONS[2].callback = return_to_title_option;

        option = option % 3;
    } else {
        OPTIONS[0].name = "option.options";
        OPTIONS[0].callback = options_option;
        OPTIONS[1].fmt = fmt_return_to_title;
        OPTIONS[1].callback = return_to_title_option;

        option = option % 2;
    }

    play_generic_sound("ui/pause", 0);
}

static void tick(UI* ui) {
    tick_options(NULL, OPTIONS, &option);
    if (kb_pressed(KB_PAUSE)) {
        play_generic_sound("ui/select", 0);
        ui->flags |= UIF_DESTROY;
    }
}

static void draw(const UI* ui) {
    (void)ui;

    batch_reset();
    batch_pos(B_F3_XY(-1000.f, -1000.f));
    batch_color(B_U4(0, 0, 0, 128));
    batch_rectangle(NULL, B_F2_S(3000.f));

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 16.f));
    batch_colors(B_U4X4_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("menu.paused"));

    draw_options(OPTIONS, option, 56.f);
}

const UITable TAB_PAUSE = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
};
