#include "K_audio.h"
#include "K_interface.h"
#include "K_video.h"

static void start(const char*) {
    load_track("title", FALSE);

    play_generic_track(GTS_MAIN, "title", PLAY_LOOPING);
}

static void draw() {
    clear_color(1.f, 1.f, 1.f, 1.f);
}

static void draw_ui() {
    batch_reset();
    batch_string("main", 24.f,
        "The quick brown fox jumps over the lazy dog.\nthe quick brown fox jumps over the lazy dog.\nTHE QUICK BROWN "
        "FOX JUMPS OVER THE LAZY DOG.");
}

const ScreenTable TAB_MENU = {
    .start = start,
    .draw = draw,
    .draw_ui = draw_ui,
};
