#pragma once

#include "K_game.h"

#define DEFAULT_NAME "Player"
#define DEFAULT_LANGUAGE "en"

#define CMD_OPT(ident) cmd_set_##ident
#define MAKE_FLAG(ident) MAKE_OPTION_PRO(ident, Bool, FALSE, TRUE)
#define MAKE_OPTION(ident, default) MAKE_OPTION_PRO(ident, const char*, default, next_arg())
#define MAKE_OPTION_PRO(ident, type, default, set_expr)                                                                \
    static type ident = default;                                                                                       \
    static void CMD_OPT(ident)() {                                                                                     \
        (ident) = (set_expr);                                                                                          \
    }

const char* next_arg();
typedef struct {
    const char *shortform, *longform;
    void (*handler)();
} CmdArg;

typedef struct {
    char name[32];
    char language[256];

    char world[256];
    char level[256];
    PlayerCharacter character;
    PlayerPowerup powerup;
    Bool xscroll, seen_xscroll_notice;
    Bool record_replay;

    Bool seen_online_notice;
    char server[256];
    Uint8 lobby_limit;
    Bool private_lobby;
    Bool show_user_messages;

    Uint8 input_delay;

    Bool texture_filter;
    Bool show_hud;

    Bool audio_in_background;
} ClientInfo;
extern ClientInfo CLIENT;

void handle_cmdline(int, char*[]);
