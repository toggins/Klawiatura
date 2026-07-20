#include "K_input.h"
#include "K_interface.h"
#include "K_net.h"
#include "K_replay.h"

static void start(const void* secret, size_t secret_size) {
    EXPECT(secret_size == sizeof(GameContext), "Secret isn't GameContext?");

    load_ui(UI_PAUSE);

    start_game((GameContext*)secret);
    start_replay();
}

static void tick() {
    Bool can_tick = TRUE;
    if (topui() == NULL && kb_pressed(KB_PAUSE)) {
        create_ui(UI_PAUSE, NULL);
        if (!is_connected())
            can_tick = FALSE;
    }

    if (can_tick)
        tick_game();

    tick_interp();
}

static void end() {
    end_replay();
    nuke_game();
}

const ScreenTable TAB_GAME = {
    .start = start,
    .tick = tick,
    .draw_ui = draw_game,
    .end = end,
};
