#include "K_audio.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_net.h"
#include "K_string.h"
#include "K_video.h"

typedef struct {
    NetID peers[MAX_PEERS];
    Option options[MAX_OPTIONS];
    size_t option;
} UIKickData;

static const char* fmt_peer(size_t idx) {
    return get_peer_name(((UIKickData*)topui()->userdata)->peers[idx]);
}

static void peer_option() {
    UI* ui = topui();
    UIKickData* userdata = ui->userdata;
    kick_peer(userdata->peers[userdata->option]);
    ui->flags |= UIF_DESTROY;
}

static void create(UI* ui) {
    UI_ALLOC_DATA(ui, UIKickData);

    UIKickData* userdata = ui->userdata;
    size_t i = 0;
    for (const NetID* ptr = get_peers(); *ptr > 0; ptr++) {
        const NetID pid = *ptr;
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
    tick_options(NULL, userdata->options, &userdata->option);

    if (kb_pressed(KB_PAUSE)) {
        play_generic_sound("ui/select", 0);
        ui->flags |= UIF_DESTROY;
    }
}

static void draw(const UI* ui) {
    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, 16.f));
    batch_colors(B_MF_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("opt_kick_player"));

    const UIKickData* userdata = ui->userdata;
    draw_options(userdata->options, userdata->option, 64.f);

    batch_reset();
    batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
    batch_colors(B_MF_BLUE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("header", 32.f, fmt("[%s] %s", kb_label(KB_PAUSE), LFMT("back")));
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
