#pragma once

#include "K_log.h"

#define MAX_MENUS 16
#define MAX_OPTIONS 16

#define OPTION_PROMPT(text) .prompt = (text), .prompt_size = sizeof(text)

#define UI_ALLOC_DATA(ui, udtype)                                                                                      \
    do {                                                                                                               \
        (ui)->userdata = SDL_calloc(1, sizeof(udtype));                                                                \
        EXPECT((ui)->userdata, "Failed to allocate " #udtype);                                                         \
    } while (FALSE)

typedef Uint8 ScreenType;
enum {
    SCR_NULL,

    SCR_LOGO,
    SCR_MENU,
    SCR_MAP,
    SCR_GAME,

    SCR_SIZE,
};

typedef struct {
    void (*start)(const void*, size_t);
    void (*tick)();
    void (*draw)(), (*draw_ui)();
    void (*end)();
} ScreenTable;

typedef Uint8 UIType;
enum {
    UI_NULL,

    UI_MESSAGE,
    UI_QUESTION,
    UI_PAUSE,
    UI_OPTIONS,
    UI_KICK,

    UI_SIZE,
};

typedef Uint8 UIFlags;
enum {
    UIF_DESTROY = 1 << 0,
    UIF_BLOCK = 1 << 1,
    UIF_MEGABLOCK = 1 << 2,
};

typedef struct UI {
    UIType type;
    UIFlags flags;

    struct UI *parent, *child;
    void* userdata;
} UI;

typedef struct {
    void (*load)();
    void (*create)(UI*);
    void (*tick)(UI*);
    void (*draw)(const UI*);
    void (*cleanup)(UI*);
} UITable;

typedef Uint8 MenuType;

typedef struct {
    const char *name, *(*fmt)(size_t);
    Bool (*disabled)();

    MenuType menu;
    void (*callback)(), (*cycle)(Sint8);

    char* prompt;
    size_t prompt_size;
    void (*submit)(Bool);
} Option;

typedef struct {
    const char *name, *(*fmt)();

    size_t option;
    MenuType from;
    Bool ghost;

    void (*enter)(MenuType), (*leave)(MenuType);
    void (*tick)();
    Bool (*draw)();
} Menu;

typedef struct {
    MenuType current;
    Menu menus[MAX_MENUS];
    Option options[MAX_MENUS][MAX_OPTIONS];
} Catalog;

void interface_init(), interface_update(), interface_teardown();
void permadeath();

ScreenType get_screen();
void set_screen(ScreenType, const void*, size_t);

void boot_to_menu(const char*);

void load_ui(UIType);
UI* create_ui(UIType, UI*);
UI *rootui(), *topui();

Bool set_menu(Catalog*, MenuType), previous_menu(Catalog*);
void tick_options(Catalog*, Option*, size_t*);
Bool tick_catalog(Catalog*, UI*);
void draw_options(const Option*, size_t, float), draw_catalog(const Catalog*);

Bool always_disabled();
