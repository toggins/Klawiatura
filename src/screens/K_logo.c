#include "K_game.h" // IWYU pragma: export
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_tick.h"
#include "K_video.h"

void start(const void* secret, size_t secret_size) {
    (void)secret;
    (void)secret_size;

    load_sprite(LFMT("logo.disclaimer"), AKL_NEVER);
    load_sprite("logos/sdl", AKL_NEVER);
    load_sprite("logos/gekkonet", AKL_NEVER);
}

// 150 + 130
void tick() {
    if (totalticks() > 280.f || kb_pressed(KB_JUMP) || kb_pressed(KB_UI_ENTER))
        set_screen(SCR_MENU, TRANS_NONE, 0.f, NULL, 0);
}

void draw() {
    clear_color(B_F4_1);
}

void draw_ui() {
    batch_reset();

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT - 64.f));
    const float t = totalticks();
    batch_color(B_U4_ALPHA(
        ((t < 33.5f) ? (t / 33.5f) : ((t > 150.f) ? ((t < 278.f) ? (1.f - ((t - 150.f) / 128.f)) : 0.f) : 1.f))
        * 255.f));
    batch_sprite(LFMT("logo.disclaimer"));

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH - 128.f, SCREEN_HEIGHT - 112.f));
    batch_scale(B_F2_S(0.5f));
    batch_sprite("logos/sdl");

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH + 128.f, SCREEN_HEIGHT - 112.f));
    batch_sprite("logos/gekkonet");
}

const ScreenTable TAB_LOGO = {
    .start = start,
    .tick = tick,
    .draw = draw,
    .draw_ui = draw_ui,
};
