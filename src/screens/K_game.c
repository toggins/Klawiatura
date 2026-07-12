#include "K_input.h"
#include "K_interface.h"
#include "K_net.h"

static void start(const void* secret, size_t secret_size) {
    EXPECT(secret_size == sizeof(GameContext), "Secret isn't GameContext?");
    start_game(secret);

    load_ui(UI_PAUSE);
}

static void tick() {
    if (topui() == NULL && kb_pressed(KB_PAUSE)) {
        create_ui(UI_PAUSE, NULL);
        if (!is_connected())
            return;
    }
}

const ScreenTable TAB_GAME = {
    .start = start,
    .tick = tick,
};
