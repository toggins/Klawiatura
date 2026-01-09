#pragma once

#include <SDL3/SDL_stdinc.h>

#define CLIENT_STRING_MAX (NUTPUNCH_FIELD_DATA_MAX)

#include "K_game.h"
#include "K_net.h" // IWYU pragma: keep

#define CMD_OPT(ident) cmd_set_##ident
#define MAKE_FLAG(ident) MAKE_OPTION_PRO(ident, Bool, false, true)
#define MAKE_OPTION(ident, default) MAKE_OPTION_PRO(ident, const char*, default, next_arg())
#define MAKE_OPTION_PRO(ident, type, default, set_expr)                                                                \
	static type ident = default;                                                                                   \
	static void CMD_OPT(ident)() {                                                                                 \
		(ident) = (set_expr);                                                                                  \
	}

const char* next_arg();
typedef struct {
	const char *shortform, *longform;
	void (*handler)();
} CmdArg;

// !!! !!! !!!
// !!! !!! !!!
// !!! !!! !!!

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_KEVIN (CLIENT.game.kevin * GF_KEVIN)

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_FRED (CLIENT.game.fred * GF_FRED)

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_HELL (GF_TRY_KEVIN | GF_TRY_FRED)

// !!! !!! !!!
// !!! !!! !!!
// !!! !!! !!!

typedef struct {
	struct {
		Bool aware;
		char name[CLIENT_STRING_MAX];
		char skin[CLIENT_STRING_MAX];
	} user;

	struct {
		Uint8 delay;
	} input;

	struct {
		Bool filter;
	} video;

	struct {
		Bool background;
	} audio;

	struct {
		PlayerID players;
		Bool kevin, fred;
		char level[CLIENT_STRING_MAX];
	} game;

	struct {
		char name[LOBBY_STRING_MAX];
		Bool public;
	} lobby;
} ClientInfo;
extern ClientInfo CLIENT;

void handle_cmdline(int, char*[]);
