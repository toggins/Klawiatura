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
    if (topui() == NULL && kb_pressed(KB_PAUSE)) {
        create_ui(UI_PAUSE, NULL);
        if (!is_connected())
            return;
    }

    tick_game();
}

static void end() {
    end_replay();
    nuke_game();
}

const ScreenTable TAB_GAME = {
    .start = start,
    .tick = tick,
    .pre_interp = pre_interp_game,
    .interp = interp_game,
    .draw_ui = draw_game,
    .end = end,
};
