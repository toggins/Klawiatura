#include "K_audio.h"
#include "K_chat.h"
#include "K_cmd.h"
#include "K_discord.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_locale.h"
#include "K_tick.h"
#include "K_video.h"

#define SCREEN_CALL_STATIC(type, fn)                                                                                   \
    do {                                                                                                               \
        if (SCREENS[(type)] != NULL && SCREENS[(type)]->fn != NULL)                                                    \
            SCREENS[(type)]->fn();                                                                                     \
    } while (FALSE)

#define SCREEN_CALL(type, fn, val)                                                                                     \
    do {                                                                                                               \
        if (SCREENS[(type)] != NULL && SCREENS[(type)]->fn != NULL)                                                    \
            SCREENS[(type)]->fn(val);                                                                                  \
    } while (FALSE)

#define UI_CALL_STATIC(type, fn)                                                                                       \
    do {                                                                                                               \
        if (UIS[(type)] != NULL && UIS[(type)]->fn != NULL)                                                            \
            UIS[(type)]->fn();                                                                                         \
    } while (FALSE)

#define UI_CALL(ui, fn)                                                                                                \
    do {                                                                                                               \
        if (UIS[(ui)->type] != NULL && UIS[(ui)->type]->fn != NULL)                                                    \
            UIS[(ui)->type]->fn(ui);                                                                                   \
    } while (FALSE)

// `extern` in S_screens.c
const ScreenTable* SCREENS[SCR_SIZE] = {NULL};

static ScreenType current_screen = SCR_NULL, to_screen = SCR_NULL;
static const char* to_secret = NULL;

// `extern` in S_uis.c
const UITable* UIS[UI_SIZE] = {NULL};

static UI *root_ui = NULL, *top_ui = NULL;

void interface_init() {
    extern void POPULATE_SCREENS_TABLE(), POPULATE_UIS_TABLE();
    POPULATE_SCREENS_TABLE();
    POPULATE_UIS_TABLE();

    load_font("main", TRUE);
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

    if (to_screen != SCR_NULL) {
        batch_reset();
        batch_pos(B_HALF_SCREEN);
        batch_align(B_CENTER);
        batch_string("main", 16.f, "LOADING");
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

    const Sint8 cycle = (Sint8)((Sint8)kb_repeated(KB_UI_RIGHT) - (Sint8)kb_repeated(KB_UI_LEFT));
    if (cycle != 0) {
        const Option* option = &options[*curopt];
        if ((option->disabled == NULL || !option->disabled()) && option->cycle != NULL)
            option->cycle(cycle);
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

    return (!kb_pressed(KB_PAUSE) || set_menu(catalog, menus[catalog->current].from));
}

void draw_options(const Option* options, size_t curopt, float y) {}

void draw_catalog(const Catalog* catalog) {}

Bool always_disabled() {
    return TRUE;
}
