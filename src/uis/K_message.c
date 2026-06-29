#include "K_audio.h"
#include "K_game.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_video.h"

#include "uis/K_message.h"

static void create(UI* ui) {
    ui->flags |= UIF_BLOCK;

    UI_ALLOC_DATA(ui, UIMessageData);
    UIMessageData* userdata = ui->userdata;
    userdata->verb = "close";
    userdata->font = "header";
    userdata->size = 32.f;
}

static void tick(UI* ui) {
    UIMessageData* userdata = ui->userdata;

    if (userdata->wait != NULL && userdata->wait()) {
        if (userdata->finish != NULL)
            userdata->finish();

        ui->flags |= UIF_DESTROY;
        return;
    }

    if ((userdata->wait == NULL || userdata->cancel != NULL) && kb_pressed(KB_PAUSE)) {
        if (userdata->cancel != NULL)
            userdata->cancel();

        play_generic_sound("ui/select", 0);
        ui->flags |= UIF_DESTROY;
    }
}

static void draw(const UI* ui) {
    batch_reset();

    if (get_screen() != SCR_MENU) {
        batch_pos(B_XY(-1000.f, -1000.f));
        batch_color(B_RGBA(0, 0, 0, 128));
        batch_rectangle(NULL, B_SIZE(3000.f));
    }

    const UIMessageData* userdata = ui->userdata;
    if (userdata == NULL)
        return;

    if (userdata->title != NULL) {
        batch_pos(B_XY(HALF_SCREEN_WIDTH, 16.f));
        batch_colors(B_MF_YELLOW);
        batch_align(B_ALIGN(FA_CENTER, FA_TOP));
        batch_string("header", 32.f, LFMT(userdata->title));
    }

    if (userdata->text != NULL || userdata->fmt != NULL) {
        batch_pos(B_HALF_SCREEN);
        batch_colors(B_MF_WHITE);
        batch_align(B_CENTER);
        batch_string_wrap(userdata->font, userdata->size,
            (userdata->fmt == NULL) ? LFMT(userdata->text) : userdata->fmt(), SCREEN_WIDTH - 32.f);
    }

    if (userdata->verb != NULL && (userdata->wait == NULL || userdata->cancel != NULL)) {
        batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
        batch_colors(B_MF_BLUE);
        batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
        batch_string("header", 32.f, fmt("[%s] %s", kb_label(KB_PAUSE), LFMT(userdata->verb)));
    }
}

const UITable TAB_MESSAGE = {
    .create = create,
    .tick = tick,
    .draw = draw,
};
