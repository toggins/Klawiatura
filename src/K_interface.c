#include <SDL3/SDL_platform_defines.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_discord.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#define SCREEN_CALL_STATIC(type, fn)                                                                                   \
    do {                                                                                                               \
        if (SCREENS[(type)] != NULL && SCREENS[(type)]->fn != NULL)                                                    \
            SCREENS[(type)]->fn();                                                                                     \
    } while (FALSE)

#define SCREEN_CALL(type, fn, ...)                                                                                     \
    do {                                                                                                               \
        if (SCREENS[(type)] != NULL && SCREENS[(type)]->fn != NULL)                                                    \
            SCREENS[(type)]->fn(__VA_ARGS__);                                                                          \
    } while (FALSE)

#define UI_CALL_STATIC(type, fn)                                                                                       \
    do {                                                                                                               \
        if (UIS[(type)] != NULL && UIS[(type)]->fn != NULL)                                                            \
            UIS[(type)]->fn();                                                                                         \
    } while (FALSE)

#define UI_CALL(ui, fn)                                                                                                \
    do {                                                                                                               \
        if (UIS[(ui)->type] != NULL && UIS[(ui)->type]->fn != NULL)                                                    \
            UIS[(ui)->type]->fn((ui));                                                                                 \
    } while (FALSE)

// `extern` in S_screens.c
const ScreenTable* SCREENS[SCR_SIZE] = {NULL};

static ScreenType current_screen = SCR_NULL, to_screen = SCR_NULL;
static const char* to_secret = NULL;

// `extern` in S_uis.c
const UITable* UIS[UI_SIZE] = {NULL};

static UI *root_ui = NULL, *top_ui = NULL;

static Uint8 cursor_frame = 0;

void interface_init() {
    extern void POPULATE_SCREENS_TABLE(), POPULATE_UIS_TABLE();
    POPULATE_SCREENS_TABLE();
    POPULATE_UIS_TABLE();

    load_sprite_num("ui/cursor/%u", 12, TRUE);
    load_font("main", TRUE);
    load_font("header", TRUE);
    load_sound("ui/switch", TRUE);
    load_sound("ui/select", TRUE);
    load_sound("ui/toggle", TRUE);

    set_screen(SCR_LOGO, NULL);
}

static void destroy_ui(UI*);

void interface_update() {
    if (to_screen == SCR_NULL)
        goto iu_dont_change;

    destroy_ui(root_ui);

    SCREEN_CALL_STATIC(current_screen, end);
    if (current_screen != to_screen) {
        audio_wipeout();
        clear_assets();
    }

    current_screen = to_screen;
    to_screen = SCR_NULL;
    SCREEN_CALL(current_screen, start, to_secret);
    update_discord_status();
    input_wipeout();
    from_scratch();

iu_dont_change:
    if (kb_pressed(KB_RECORD_REPLAY)) {
        CLIENT.record_replay = !CLIENT.record_replay;
        if (CLIENT.record_replay)
            chat_message(LFMT("chat_recording_soon"), B_GREEN);
        else
            chat_message(LFMT("chat_recording_cancelled"), B_RED);
    }

    poll_game();
    for (new_frame(); got_ticks(); next_tick()) {
        // Chat
        chat_update();

        // Cursor
        if (cursor_frame > 0) {
            cursor_frame += 6;
            if (cursor_frame >= 120)
                cursor_frame = 0;
        } else if (SDL_fmodf(totalticks(), 5.f) < 1.f && SDL_rand(20) == 10) {
            ++cursor_frame;
        }

        // UI
        UIFlags should_block = 0;
        UI* ui = root_ui;
        while (ui != NULL) {
            if (ui->flags & UIF_DESTROY) {
                UI* killed = ui;
                ui = killed->parent;
                destroy_ui(killed);
                continue;
            }

            should_block |= ui->flags & (UIF_BLOCK | UIF_MEGABLOCK);

            if (ui->child == NULL) {
                UI_CALL(ui, tick);
                break;
            }

            ui = ui->child;
        }

        // Screen
        if (((should_block & UIF_BLOCK) && !is_connected()) || (should_block & UIF_MEGABLOCK)) {
            pause_audio_state(TRUE);
        } else {
            pause_audio_state(FALSE);
            SCREEN_CALL_STATIC(current_screen, tick);
        }

        // Transition
        if (to_screen != SCR_NULL)
            break;
    }

    start_drawing();
    SCREEN_CALL_STATIC(current_screen, draw);
    start_drawing_ui();
    SCREEN_CALL_STATIC(current_screen, draw_ui);
    if (top_ui != NULL)
        UI_CALL(top_ui, draw);
    draw_chat();

    if (to_screen == SCR_NULL) {
#ifdef SDL_PLATFORM_EMSCRIPTEN
        if (!window_focused()) {
            batch_reset();
            batch_color(B_RGBA(0, 0, 0, 128));
            batch_rectangle(NULL, B_SCREEN);
            batch_pos(B_HALF_SCREEN);
            batch_color(B_WHITE);
            batch_align(B_CENTER);
            batch_string("main", 24.f, LFMT("click_to_focus"));
        }
#endif
    } else {
        batch_reset();
        batch_pos(B_HALF_SCREEN);
        batch_align(B_CENTER);
        batch_string("main", 24.f, "LOADING");
    }

    stop_drawing();
}

void interface_teardown() {
    destroy_ui(root_ui);
    SCREEN_CALL_STATIC(current_screen, end);
}

void permadeath() {
    SDL_PushEvent(&(SDL_Event){.type = SDL_EVENT_QUIT});
}

ScreenType get_screen() {
    return current_screen;
}

void set_screen(ScreenType type, const char* secret) {
    ASSUME(type > SCR_NULL && type < SCR_SIZE, "Going to invalid screen %u?", type);
    to_screen = type;
    to_secret = secret;
}

void load_ui(UIType type) {
    WHATEVER(type > UI_NULL && type < UI_SIZE, "Loading invalid type %u", type);
    UI_CALL_STATIC(type, load);
}

// NOLINTBEGIN(misc-no-recursion)
static void destroy_ui(UI* ui) {
    if (ui == NULL)
        return;

    destroy_ui(ui->child);
    UI_CALL(ui, cleanup);

    if (root_ui == ui)
        root_ui = NULL;
    if (top_ui == ui)
        top_ui = ui->parent;
    if (ui->parent != NULL)
        ui->parent->child = NULL;

    SDL_free(ui->userdata);
    SDL_free(ui);
}
// NOLINTEND(misc-no-recursion)

UI* create_ui(UIType type, UI* parent) {
    if (type <= UI_NULL || type >= UI_SIZE) {
        WTF("Invalid UI type %u", type);
        return NULL;
    }

    UI* ui = SDL_calloc(1, sizeof(*ui));
    EXPECT(ui, "Failed to allocate UI %u", type);

    ui->type = type;

    if (parent == NULL) {
        if (root_ui != NULL) {
            root_ui->flags |= UIF_DESTROY;
            ui->child = root_ui;
            root_ui->parent = ui;
        }
        root_ui = top_ui = ui;
    } else {
        if (parent->child != NULL) {
            parent->child->flags |= UIF_DESTROY;
            if (top_ui == parent->child)
                top_ui = ui;
            ui->child = parent->child;
        } else if (top_ui == parent) {
            top_ui = ui;
        }
        parent->child = ui;
        ui->parent = parent;
    }

    UI_CALL(ui, create);
    return ui;
}

UI* rootui() {
    return root_ui;
}

UI* topui() {
    return top_ui;
}

Bool set_menu(Catalog* catalog, MenuType to) {
    if (to <= 0 || to >= MAX_MENUS)
        return FALSE;

    Menu* menus = catalog->menus;
    if (menus[catalog->current].leave != NULL)
        menus[catalog->current].leave(to);
    if (menus[catalog->current].from != to && catalog->current != to)
        menus[to].from = menus[catalog->current].ghost ? menus[catalog->current].from : catalog->current;
    if (menus[to].enter != NULL)
        menus[to].enter(catalog->current);
    catalog->current = to;

    // Try finding first valid option
    size_t i = menus[catalog->current].option, j = MAX_OPTIONS;
    while (j > 0) {
        const Option* option = &catalog->options[catalog->current][i];
        if ((option->name != NULL || option->fmt != NULL) && (option->disabled == NULL || !option->disabled()))
            break;
        i = (i + 1) % MAX_OPTIONS;
        --j;
    }
    menus[catalog->current].option = i;

    return TRUE;
}

Bool previous_menu(Catalog* catalog) {
    return set_menu(catalog, catalog->menus[catalog->current].from);
}

void tick_options(Catalog* catalog, Option* options, size_t* curopt) {
    const size_t oldopt = *curopt;

#define CHECKOPT                                                                                                       \
    const Option* option = &options[*curopt];                                                                          \
    if ((option->name == NULL && option->fmt == NULL) || (option->disabled != NULL && option->disabled())) {           \
        ++i;                                                                                                           \
        continue;                                                                                                      \
    }

    Sint8 change = (Sint8)((Sint8)kb_repeated(KB_UI_DOWN) - (Sint8)kb_repeated(KB_UI_UP));
    Uint8 i = 0;
    while (change < 0 && i < MAX_OPTIONS) {
        if (*curopt <= 0)
            *curopt = MAX_OPTIONS - 1;
        else
            --*curopt;
        CHECKOPT;
        ++change;
    }
    while (change > 0 && i < MAX_OPTIONS) {
        if (*curopt >= (MAX_OPTIONS - 1))
            *curopt = 0;
        else
            ++*curopt;
        CHECKOPT;
        --change;
    }

#undef CHECKOPT

    if (oldopt != *curopt)
        play_generic_sound("ui/switch", 0);

    const Sint8 cycle = (Sint8)((Sint8)kb_repeated(KB_UI_RIGHT) - (Sint8)kb_repeated(KB_UI_LEFT));
    if (cycle != 0) {
        const Option* option = &options[*curopt];
        if ((option->disabled == NULL || !option->disabled()) && option->cycle != NULL) {
            option->cycle(cycle);
            play_generic_sound("ui/toggle", 0);
        }
    }

    if (kb_pressed(KB_UI_ENTER)) {
        const Option* option = &options[*curopt];
        if (option->disabled == NULL || !option->disabled()) {
            set_menu(catalog, option->menu);

            if (option->callback != NULL)
                option->callback();
            else if (option->cycle != NULL)
                option->cycle(1);

            if (option->prompt != NULL)
                start_typing(option->prompt, option->prompt_size, option->submit);

            play_generic_sound("ui/select", 0);
        }
    }
}

Bool tick_catalog(Catalog* catalog, UI* in) {
    if (top_ui != in)
        return TRUE;

    Menu* menus = catalog->menus;

    MenuType last_menu = catalog->current;
    if (menus[catalog->current].tick != NULL)
        menus[catalog->current].tick();
    if (last_menu != catalog->current)
        return TRUE;

    Option* options = catalog->options[catalog->current];
    tick_options(catalog, options, &menus[catalog->current].option);

    if (kb_pressed(KB_PAUSE)) {
        if (set_menu(catalog, menus[catalog->current].from)) {
            play_generic_sound("ui/select", 0);
        } else if (in != NULL) {
            play_generic_sound("ui/select", 0);
            return FALSE;
        }
    }

    return TRUE;
}

void draw_options(const Option* options, size_t curopt, float y) {
    static const float OPTION_WRAP = SCREEN_WIDTH - 64.f;

    batch_reset();
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));

    float oy = y;
    for (size_t i = 0; i < MAX_OPTIONS; i++) {
        const Option* option = &options[i];
        if (option->name == NULL && option->fmt == NULL) {
            oy += 32.f;
            continue;
        }

        const char* ostr = (option->fmt == NULL) ? LFMT(option->name) : option->fmt(i);
        const Bool disabled = option->disabled != NULL && option->disabled();

        batch_pos(B_XY(HALF_SCREEN_WIDTH, oy));
        batch_colors(disabled ? B_MF_GRAY : ((i == curopt) ? B_MF_PINK : B_MF_WHITE));
        batch_string_wrap("header", 32.f, ostr, OPTION_WRAP);

        const float oh = string_height_wrap("header", 32.f, ostr, OPTION_WRAP);
        if (i == curopt && !disabled) {
            batch_pos(B_XY(
                HALF_SCREEN_WIDTH - (float)((int)(string_width_wrap("header", 32.f, ostr, OPTION_WRAP) * 0.5f)) - 16.f,
                oy + (oh * 0.5f)));
            batch_color(B_WHITE);
            batch_sprite(fmt("ui/cursor/%u", cursor_frame / 10));
        }

        oy += oh;
    }
}

void draw_catalog(const Catalog* catalog) {
    const Menu* menu = &catalog->menus[catalog->current];
    if (menu->draw != NULL && !menu->draw())
        return;

    if (menu->name != NULL || menu->fmt != NULL) {
        const char* mstr = (menu->fmt == NULL) ? LFMT(menu->name) : menu->fmt();

        batch_reset();
        batch_pos(B_XY(HALF_SCREEN_WIDTH, 16.f));
        batch_colors(B_MF_YELLOW);
        batch_align(B_ALIGN(FA_CENTER, FA_TOP));
        batch_string("header", 32.f, mstr);
    }

    draw_options(catalog->options[catalog->current], menu->option, 56.f);

    if (menu->from > 0 || topui() != NULL) {
        batch_reset();
        batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
        batch_colors(B_MF_BLUE);
        batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
        batch_string("header", 32.f,
            (typing_what() == NULL || typing_in_chat())
                ? fmt("[%s] %s", kb_label(KB_PAUSE), LFMT((scanning_what() == NULL_KEYBIND) ? "back" : "cancel"))
                : fmt("[%s] %s\n[%s] %s", SDL_GetScancodeName(SDL_SCANCODE_RETURN), LFMT("submit"),
                      SDL_GetScancodeName(SDL_SCANCODE_ESCAPE), LFMT("cancel")));
    }
}

Bool always_disabled() {
    return TRUE;
}
