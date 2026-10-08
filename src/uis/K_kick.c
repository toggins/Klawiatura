#include "K_audio.h"
#include "K_input.h"
#include "K_string.h"
#include "K_video.h"

#include "uis/K_kick.h"

static const char* fmt_peer(size_t idx) {
    return get_peer_name(((UIKickData*)topui()->userdata)->peers[idx]);
}

static void peer_option() {
    UI* ui = topui();

    UIKickData* userdata = ui->userdata;
    if (userdata->promote)
        promote_peer(userdata->peers[userdata->option]);
    else
        kick_peer(userdata->peers[userdata->option]);

    ui->flags |= UIF_DESTROY;
}

static void create(UI* ui) {
    UI_ALLOC_DATA(ui, UIKickData);

    UIKickData* userdata = ui->userdata;
    size_t i = 0;
    for (const NetID* pids = get_peers(); *pids > 0; pids++) {
        const NetID pid = *pids;
        if (get_local_peer() == pid || get_master_peer() == pid)
            continue;

        Option* option = &userdata->options[i];
        option->name = SDL_strdup(get_peer_name(pid));
        option->callback = peer_option;
        userdata->peers[i] = pid;

        if (++i >= SDL_min(MAX_PEERS, MAX_OPTIONS))
            break;
    }
}

static void tick(UI* ui) {
    if (!is_host())
        ui->flags |= UIF_DESTROY;

    UIKickData* userdata = ui->userdata;
    tick_options(NULL, userdata->options, &userdata->option, 0);

    if (kb_pressed(KB_PAUSE)) {
        play_generic_sound("ui/select", 0);
        ui->flags |= UIF_DESTROY;
    }
}

static void draw(const UI* ui) {
    batch_reset();

    if (get_screen() != SCR_MENU) {
        batch_pos(B_F3_XY(-1000.f, -1000.f));
        batch_color(B_U4(0, 0, 0, 128));
        batch_rectangle(NULL, B_F2_S(3000.f));
    }

    const UIKickData* userdata = ui->userdata;

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 16.f));
    batch_colors(B_U4X4_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT(userdata->promote ? "option.promote_player" : "option.kick_player"));

    draw_options(userdata->options, userdata->option, 64.f);

    batch_reset();
    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("footer", 16.f,
        fmt("[%s] %s   [%s] %s", kb_label(KB_UI_ENTER), LFMT(userdata->promote ? "menu.promote" : "menu.kick"),
            kb_label(KB_PAUSE), LFMT("menu.back")));
}

static void cleanup(UI* ui) {
    UIKickData* userdata = ui->userdata;
    for (size_t i = 0; i < MAX_OPTIONS; i++)
        SDL_free((void*)userdata->options[i].name);
}

const UITable TAB_KICK = {
    .create = create,
    .tick = tick,
    .draw = draw,
    .cleanup = cleanup,
};
