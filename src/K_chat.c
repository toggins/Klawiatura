#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_input.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#define MAX_PROMPT 100
#define MAX_LINES 5

#define CHAT_SIZE 24

static char prompt[MAX_PROMPT] = {0};
static struct {
    Uint8 color[4];
    Uint16 time;
    const char* str;
} lines[MAX_LINES] = {0};

void chat_init() {
    load_sound("ui/chat", TRUE);
}

static void submit_chat_message(Bool confirmed) {
    if (!confirmed || prompt[0] == '\0')
        return;

    const int len = (int)SDL_strnlen(prompt, sizeof(prompt));
    if (len <= 0)
        return;

    const int size = (int)sizeof(PacketType) + len;
    Uint8* data = net_buffer();
    *(PacketType*)data = PT_CHAT;
    SDL_memcpy((char*)(data + sizeof(PacketType)), prompt, len);
    spread_reliable_packet(PCH_LOBBY, data, size);

    chat_message(fmt("%s: %s", CLIENT.name, prompt), B_WHITE);
}

void chat_update() {
    for (size_t i = 0; i < MAX_LINES; i++)
        if (lines[i].time > 0)
            --lines[i].time;

    if (!CLIENT.show_user_messages) {
        if (typing_what() == prompt)
            stop_typing();
    }

    if (kb_pressed(KB_CHAT)) {
        if (CLIENT.show_user_messages) {
            prompt[0] = '\0';
            start_typing(prompt, sizeof(prompt), submit_chat_message);
        } else {
            chat_message(LFMT("chat_hidden"), B_GRAY);
        }
    }
}

void chat_teardown() {
    clear_chat();
}

Bool typing_in_chat() {
    return typing_what() == prompt;
}

void chat_message(const char* str, const Uint8 color[4]) {
    if (str == NULL)
        return;

    SDL_free((void*)lines[MAX_LINES - 1].str);
    for (size_t i = MAX_LINES - 1; i > 0; i--)
        lines[i] = lines[i - 1];

    SDL_memcpy(lines[0].color, color, sizeof(lines[0].color));
    lines[0].time = 6 * TICKRATE;
    lines[0].str = SDL_strdup(str);

    if (SDL_memcmp(color, B_WHITE, 4) == 0)
        play_generic_sound("ui/chat", PLAY_SYSTEM);

    INFO("%s", lines[0].str);
}

void clear_chat() {
    for (size_t i = 0; i < MAX_LINES; i++) {
        SDL_free((void*)lines[i].str);
        lines[i].str = NULL;
    }
}

void draw_chat() {
    const Bool typing = typing_what() == prompt;

    batch_reset();
    batch_align(B_BOTTOM_LEFT);

    float y = SCREEN_HEIGHT - 24.f - CHAT_SIZE;
    for (size_t i = 0; i < MAX_LINES; i++) {
        const char* lstr = lines[i].str;
        if (lstr == NULL)
            break;

        const float lh = string_height_wrap("main", CHAT_SIZE, lstr, SCREEN_WIDTH - 32.f);
        if (!typing && (lines[i].time <= 0 || (y - lh) < (SCREEN_HEIGHT - 24.f - ((MAX_LINES + 1) * CHAT_SIZE))))
            break;

        const Uint8* color = lines[i].color;
        const float a
            = (typing ? 1.f : (0.75f * (SDL_min(lines[i].time, (float)TICKRATE) / (float)TICKRATE))) * (float)color[3];

        batch_pos(B_XY(16.f, y));
        batch_color(B_RGBA(color[0], color[1], color[2], a));
        batch_string_wrap("main", CHAT_SIZE, lstr, SCREEN_WIDTH - 32.f);

        y -= lh;
    }

    if (typing) {
        const char* lstr = fmt("%s%s", prompt, caret(TRUE));
        const float xmax = SCREEN_WIDTH - 32.f - string_width("main", CHAT_SIZE, prompt);
        const float x = SDL_min(16.f, xmax);

        batch_pos(B_XY(x, SCREEN_HEIGHT - 16.f));
        batch_color(B_WHITE);
        batch_string("main", CHAT_SIZE, lstr);
    }

    set_shader(SH_MAIN);
}
