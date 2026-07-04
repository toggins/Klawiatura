#include "K_audio.h"
#include "K_game.h" // IWYU pragma: export
#include "K_input.h"
#include "K_interface.h"
#include "K_tick.h"
#include "K_video.h"

void start(const void* secret, size_t secret_size) {
    (void)secret;
    (void)secret_size;

    load_sprite("logos/buziol", FALSE);
    load_sound("logo", FALSE);
    load_sound("logo2", FALSE);

    play_generic_sound("logo", 0);
    play_generic_sound("logo2", 0);
}

void tick() {
    if (totalticks() > 280.f || kb_pressed(KB_UI_ENTER))
        set_screen(SCR_MENU, NULL, 0);
}

void draw() {
    clear_color(1.f, 1.f, 1.f, 1.f);
}

void draw_ui() {
    batch_reset();
    batch_pos(B_HALF_SCREEN);

    const float t = totalticks();
    batch_color(
        B_ALPHA(((t < 33.5f) ? (t / 33.5f) : ((t > 235.f) ? ((t < 280.f) ? (1.f - ((t - 235.f) / 45.f)) : 0.f) : 1.f))
                * 255.f));

    batch_alpha_test(0.9f);
    batch_sprite("logos/buziol");
    batch_alpha_test(0.f);
}

const ScreenTable TAB_LOGO = {
    .start = start,
    .tick = tick,
    .draw = draw,
    .draw_ui = draw_ui,
};
